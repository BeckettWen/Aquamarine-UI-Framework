#pragma once

// Stable renderer entry point. Platform selection is a build-time decision;
// the rectangle, text, animation, texture, and input APIs remain the same.
#if defined(LYRA_USE_METAL)
#include "MetalRenderer.hpp"
using Renderer = MetalRenderer;
#else
#include "VulkanRenderer.hpp"
using Renderer = VulkanRenderer;
#endif
