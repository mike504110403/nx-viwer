cmake_minimum_required(VERSION 3.15)
project(NxViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== SDL2 ==========
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(/opt/homebrew/include/SDL2) # macOS 手動指定 SDL2 headers

# ========== OpenGL ==========
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

# ========== GLEW ==========
find_path(GLEW_INCLUDE_DIR GL/glew.h PATHS /opt/homebrew/include)
find_library(GLEW_LIBRARY GLEW PATHS /opt/homebrew/lib)
include_directories(${GLEW_INCLUDE_DIR})

# ========== LZ4 ==========
find_path(LZ4_INCLUDE_DIR lz4.h PATHS /opt/homebrew/include)
find_library(LZ4_LIBRARY NAMES lz4 PATHS /opt/homebrew/lib)
include_directories(${LZ4_INCLUDE_DIR})

# ========== nlohmann/json (header-only) ==========
include_directories(external)

# ========== ImGui ==========
add_subdirectory(external/imgui)
include_directories(
    external/imgui
    external/imgui/backends
)

# ========== NLNX loader (external/nlnx/) ==========
file(GLOB NLNX_SRC
    external/nlnx/*.cpp
)
include_directories(external/nlnx)

# ========== Source files ==========
set(SOURCES
    src/main.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
    ${NLNX_SRC}
)

# ========== Build Executable ==========
add_executable(NxViewer ${SOURCES})

target_link_libraries(NxViewer
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARY}
    ${LZ4_LIBRARY}
    imgui
)

# ========== macOS Runtime Info ==========
if(APPLE)
    message(STATUS "🖥 Building for macOS")
    set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo ${CMAKE_EXE_LINKER_FLAGS}")
endif()
