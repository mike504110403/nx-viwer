#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <nlnx/nx.hpp>
#include <nlnx/node.hpp>
#include <nlnx/file.hpp>
#include <nlnx/bitmap.hpp>
#include <nlnx/audio.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <optional>
#include <unordered_map>
#include <algorithm> // for std::transform

std::unordered_map<std::string, GLuint> g_texture_cache;
float g_image_scale = 2.0f;
char g_search_query[128] = "";
std::vector<std::string> g_search_results;

void search_nodes(const nl::node &node, const std::string &path, const std::string &query)
{
    std::string lower_name = node.name();
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    if (lower_name.find(lower_query) != std::string::npos)
    {
        g_search_results.push_back(path);
    }
    for (auto it = node.begin(); it != node.end(); ++it)
    {
        std::string child_path = path + "/" + (*it).name();
        search_nodes(*it, child_path, query);
    }
}

void render_nx_node(const nl::node &node, const std::string &path, std::string &selected_path)
{
    ImGuiTreeNodeFlags flags = node.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;
    bool open = ImGui::TreeNodeEx(path.c_str(), flags, "%s", node.name().c_str());

    if (ImGui::IsItemClicked())
    {
        selected_path = path;
    }

    if (open)
    {
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            std::string child_path = path + "/" + (*it).name();
            render_nx_node(*it, child_path, selected_path);
        }
        ImGui::TreePop();
    }
}

void render_ui_tree(std::string &selected_path)
{
    std::vector<std::pair<std::string, nl::node>> groups = {
        {"Base.nx", nl::nx::base}, {"Character.nx", nl::nx::character}, {"Effect.nx", nl::nx::effect}, {"Etc.nx", nl::nx::etc}, {"Item.nx", nl::nx::item}, {"Map.nx", nl::nx::map}, {"Map001.nx", nl::nx::map001}, {"Mob.nx", nl::nx::mob}, {"Morph.nx", nl::nx::morph}, {"Npc.nx", nl::nx::npc}, {"Quest.nx", nl::nx::quest}, {"Reactor.nx", nl::nx::reactor}, {"Skill.nx", nl::nx::skill}, {"Sound.nx", nl::nx::sound}, {"String.nx", nl::nx::string}, {"TamingMob.nx", nl::nx::tamingmob}, {"UI.nx", nl::nx::ui}};

    for (const auto &[label, root] : groups)
    {
        if (!root)
            continue;

        if (ImGui::TreeNode(label.c_str()))
        {
            for (auto it = root.begin(); it != root.end(); ++it)
            {
                std::string root_path = label + "/" + (*it).name();
                render_nx_node(*it, root_path, selected_path);
            }
            ImGui::TreePop();
        }
    }
}

std::optional<nl::node> resolve_path(const std::string &path)
{
    std::istringstream iss(path);
    std::string root_file, part;
    std::getline(iss, root_file, '/');

    nl::node root;
    if (root_file == "Base.nx")
        root = nl::nx::base;
    else if (root_file == "Character.nx")
        root = nl::nx::character;
    else if (root_file == "Effect.nx")
        root = nl::nx::effect;
    else if (root_file == "Etc.nx")
        root = nl::nx::etc;
    else if (root_file == "Item.nx")
        root = nl::nx::item;
    else if (root_file == "Map.nx")
        root = nl::nx::map;
    else if (root_file == "Map001.nx")
        root = nl::nx::map001;
    else if (root_file == "Mob.nx")
        root = nl::nx::mob;
    else if (root_file == "Morph.nx")
        root = nl::nx::morph;
    else if (root_file == "Npc.nx")
        root = nl::nx::npc;
    else if (root_file == "Quest.nx")
        root = nl::nx::quest;
    else if (root_file == "Reactor.nx")
        root = nl::nx::reactor;
    else if (root_file == "Skill.nx")
        root = nl::nx::skill;
    else if (root_file == "Sound.nx")
        root = nl::nx::sound;
    else if (root_file == "String.nx")
        root = nl::nx::string;
    else if (root_file == "TamingMob.nx")
        root = nl::nx::tamingmob;
    else if (root_file == "UI.nx")
        root = nl::nx::ui;
    else
        return std::nullopt;

    while (std::getline(iss, part, '/'))
    {
        if (part.empty())
            continue;
        root = root[part];
        if (!root)
            return std::nullopt;
    }

    return root;
}

void render_image_preview(const std::string &path)
{
    auto node_opt = resolve_path(path);
    if (!node_opt)
    {
        ImGui::Text("❌ Resolve failed");
        return;
    }

    nl::node node = *node_opt;
    if (node.data_type() != nl::node::type::bitmap)
    {
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            if ((*it).data_type() == nl::node::type::bitmap)
            {
                node = *it;
                break;
            }
        }
    }

    if (node.data_type() != nl::node::type::bitmap)
    {
        ImGui::Text("❌ Not a bitmap at: %s", path.c_str());
        return;
    }

    if (g_texture_cache.find(path) == g_texture_cache.end())
    {
        auto bitmap = node.get_bitmap();
        if (!bitmap.data())
        {
            ImGui::Text("❌ Empty bitmap");
            return;
        }

        GLuint tex_id;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width(), bitmap.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        g_texture_cache[path] = tex_id;
    }

    GLuint tex_id = g_texture_cache[path];
    ImVec2 size = ImVec2(64 * g_image_scale, 64 * g_image_scale);
    ImGui::Image((ImTextureID)(intptr_t)tex_id, size);
}

int main(int, char **)
{
    nl::nx::load_all("../assets");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::cerr << "SDL Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow("NxViewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    glewInit();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 120");

    bool running = true;
    std::string selected_node_path;

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(1050, 720), ImGuiCond_FirstUseEver);
        ImGui::Begin("Nx Viewer");

        ImGui::InputText("Search##node", g_search_query, sizeof(g_search_query));
        if (ImGui::Button("Search"))
        {
            g_search_results.clear();
            std::vector<std::pair<std::string, nl::node>> groups = {
                {"Base.nx", nl::nx::base}, {"Character.nx", nl::nx::character}, {"Effect.nx", nl::nx::effect}, {"Etc.nx", nl::nx::etc}, {"Item.nx", nl::nx::item}, {"Map.nx", nl::nx::map}, {"Map001.nx", nl::nx::map001}, {"Mob.nx", nl::nx::mob}, {"Morph.nx", nl::nx::morph}, {"Npc.nx", nl::nx::npc}, {"Quest.nx", nl::nx::quest}, {"Reactor.nx", nl::nx::reactor}, {"Skill.nx", nl::nx::skill}, {"Sound.nx", nl::nx::sound}, {"String.nx", nl::nx::string}, {"TamingMob.nx", nl::nx::tamingmob}, {"UI.nx", nl::nx::ui}};
            for (const auto &[label, root] : groups)
            {
                if (root)
                    search_nodes(root, label, g_search_query);
            }
        }

        if (!g_search_results.empty())
        {
            ImGui::Text("%d results:", (int)g_search_results.size());
            for (const auto &path : g_search_results)
            {
                if (ImGui::Selectable(path.c_str()))
                {
                    selected_node_path = path;
                }
            }
            ImGui::Separator();
        }

        ImGui::Columns(2);
        ImGui::BeginChild("Tree", ImVec2(0, 0), true);
        render_ui_tree(selected_node_path);
        ImGui::EndChild();

        ImGui::NextColumn();
        ImGui::BeginChild("Preview", ImVec2(0, 0), true);

        if (!selected_node_path.empty())
        {
            render_image_preview(selected_node_path);
            ImGui::TextWrapped("%s", selected_node_path.c_str());
            ImGui::SliderFloat("Scale", &g_image_scale, 0.1f, 8.0f);
        }
        else
        {
            ImGui::Text("Select a node to preview.");
        }
        ImGui::EndChild();

        ImGui::Columns(1);
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    for (auto &[path, tex_id] : g_texture_cache)
    {
        glDeleteTextures(1, &tex_id);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
