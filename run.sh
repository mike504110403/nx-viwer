#!/bin/bash

set -e  # 發生錯誤就中止腳本

echo "🧼 Cleaning previous build directory..."
rm -rf build

echo "📁 Creating new build directory..."
mkdir build && cd build

echo "⚙️  Running CMake..."
cmake ..

echo "🔨 Building NxViewer..."
make -j$(sysctl -n hw.logicalcpu)

echo "🚀 Launching NxViewer..."
./NxViewer
