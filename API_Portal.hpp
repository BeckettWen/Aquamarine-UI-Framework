#pragma once

#include "Version.hpp"

// X-19 (Lyra) public API portal — Vulkan-based backend.
//
// The framework is now Vulkan-backed. The public surface is:
//   - Window        : GLFW window (Vulkan, no OpenGL context) + size/cursor
//   - VulkanRenderer: Vulkan device/swapchain/pipeline + batched rectangle
//                     and dedicated text rendering + click dispatch
//   - Rectangle     : widget with top-left origin, color, click response,
//                     linear/cubic-Bezier animation, and optional backdrop blur
//   - TextLabel      : baseline-positioned label rendered from the dedicated
//                     FreeType glyph texture array; optional backdrop blur
//
// Include the implementation headers directly. This header is kept as the
// project's API entry point for compatibility with the original layout.

#include "Window.hpp"
#include "Rectangle.hpp"
#include "Text.hpp"
#include "VulkanRenderer.hpp"
