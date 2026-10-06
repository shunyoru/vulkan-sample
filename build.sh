#!/bin/bash
# 編譯 Vulkan Triangle 的便利腳本

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# 若 build 目錄不存在，先執行 CMake configure
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    echo "🔧 Configuring CMake..."
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if [ $? -ne 0 ]; then
        echo "❌ CMake configure failed"
        exit 1
    fi
fi

echo "🔨 Building..."
cmake --build "${BUILD_DIR}" --parallel

if [ $? -eq 0 ]; then
    # 把 compile_commands.json symlink 到專案根目錄，讓 VSCode 找到
    ln -sf "${BUILD_DIR}/compile_commands.json" "${SCRIPT_DIR}/compile_commands.json"
    echo ""
    echo "✅ Build successful! Run with: ./run.sh"
else
    echo ""
    echo "❌ Build failed"
    exit 1
fi
