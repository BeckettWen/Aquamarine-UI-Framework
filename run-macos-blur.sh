#!/bin/sh
# Configure, build, and launch the optional macOS system-window blur test.
# Requires an installed CMake, Xcode, and Vulkan SDK; this script installs nothing.

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$PROJECT_DIR"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "run-macos-blur.sh only runs on macOS." >&2
    exit 1
fi

if [ -n "${VULKAN_ROOT:-}" ]; then
    VULKAN_SDK=${VULKAN_SDK:-"${VULKAN_ROOT%/macOS}"}
else
    VULKAN_SDK=${VULKAN_SDK:-/Users/beckett/VulkanSDK/1.4.350.1}
    VULKAN_ROOT="$VULKAN_SDK/macOS"
fi
BUILD_DIR=${LYRA_MACOS_BLUR_BUILD_DIR:-build-macos-blur}

if [ ! -x "$VULKAN_ROOT/bin/glslc" ]; then
    echo "Cannot find Vulkan SDK glslc at: $VULKAN_ROOT/bin/glslc" >&2
    echo "Set VULKAN_SDK or VULKAN_ROOT to your installed Vulkan SDK." >&2
    exit 1
fi
if [ ! -f "$VULKAN_ROOT/share/vulkan/icd.d/MoltenVK_icd.json" ]; then
    echo "Cannot find MoltenVK ICD manifest under: $VULKAN_ROOT" >&2
    exit 1
fi

cmake -G Xcode -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DVulkan_ROOT="$VULKAN_ROOT" \
    -DLYRA_ENABLE_BACKDROP_BLUR=ON \
    -DLYRA_ENABLE_MACOS_WINDOW_BLUR=ON
cmake --build "$BUILD_DIR" --config Debug --target x19 -- -quiet

export VULKAN_SDK
export DYLD_LIBRARY_PATH="$VULKAN_ROOT/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
export VK_ICD_FILENAMES="$VULKAN_ROOT/share/vulkan/icd.d/MoltenVK_icd.json"

echo "Launching macOS backdrop test. Check the window and renderer messages above."
echo "If the system blur is supported, content behind the window should show through blurred transparent areas."
exec "$BUILD_DIR/Debug/x19" "$@"
