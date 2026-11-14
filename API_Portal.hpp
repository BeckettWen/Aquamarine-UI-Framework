#pragma once

#include "Version.hpp"

// X-19 (Lyra) public API portal — platform-selected Metal or Vulkan backend.
//
// Public surface:
//   - Window        : GLFW native window + size/cursor/input dispatch
//   - Renderer      : Swift/Metal on macOS; Vulkan on other platforms
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
#include "Renderer.hpp"
