#!/bin/bash
# 執行 Vulkan Triangle 的便利腳本
# 設定 macOS / Homebrew 上 Vulkan 所需的環境變數

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BREW_PREFIX="$(brew --prefix)"

export VK_ICD_FILENAMES="${BREW_PREFIX}/Cellar/molten-vk/1.4.2/etc/vulkan/icd.d/MoltenVK_icd.json"
export VK_LAYER_PATH="${BREW_PREFIX}/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d"
export DYLD_LIBRARY_PATH="${BREW_PREFIX}/opt/vulkan-validationlayers/lib:${BREW_PREFIX}/opt/vulkan-loader/lib:${DYLD_LIBRARY_PATH}"

echo "🔺 Starting Vulkan Triangle..."
echo "   ICD:        $VK_ICD_FILENAMES"
echo "   Layer path: $VK_LAYER_PATH"
echo ""

"${SCRIPT_DIR}/build/VulkanTriangle" "$@"
