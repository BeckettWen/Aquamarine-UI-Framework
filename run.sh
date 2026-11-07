#!/bin/sh
# Run the X-19 Vulkan demo without needing to set environment variables manually.
# Sets up the Vulkan SDK paths for macOS (MoltenVK) and launches build/x19.

cd "$(dirname "$0")"   # ensure we run from the project root (finds Shaders/)

export VULKAN_SDK="/Users/beckett/VulkanSDK/1.4.350.1"
export DYLD_LIBRARY_PATH="$VULKAN_SDK/macOS/lib:${DYLD_LIBRARY_PATH:-}"
export VK_ICD_FILENAMES="$VULKAN_SDK/macOS/share/vulkan/icd.d/MoltenVK_icd.json"

exec ./build/x19 "$@"
