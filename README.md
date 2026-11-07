# Lyra

### First version: Cloudsdale

This is a **Vulkan-based rewrite** of X-19 (the design document calls it
X-19 Lyra). It is a general-purpose rendering engine whose pipeline accepts
arrays of vertices/indices and is designed to reduce draw calls.

**Backend: Vulkan** (MoltenVK on macOS). OpenGL/GLEW are no longer part of the
active build.

## First vertical slice (implemented and verified where the environment permits)

- GLFW window configured for Vulkan (no OpenGL context).
- Vulkan instance, surface, physical/logical device, graphics+present queue,
  swapchain, image views, graphics pipeline, command resources, synchronization,
  and orderly teardown.
- A visible filled rectangle (top-left-origin position/size convention,
  documented color).
- Rectangle widgets that can register a click callback.
- Pointer hit-testing using the **actual current window/framebuffer
  dimensions**, with resize handling and GLFW's top-left-origin cursor
  coordinates. Rendering and input share the same convention (no y-inversion).
- Deterministic overlap rule: **latest-added / topmost first**, consistently
  implemented for both drawing and click dispatch.
- Batching: contiguous rectangle vertex/index data submitted as **one indexed
  batch draw call** for multiple rectangles (vertex layout, indices, color
  data, and shader inputs all agree).
- Callback semantics: the registered callback fires **once** for a click inside
  the rectangle; it does not fire for clicks outside, cursor motion, or
  unrelated rectangles.
- Clean shutdown on window close with correct Vulkan/GLFW destruction order.

## Repository layout

```
Shaders/
  triangle.vert        # GLSL vertex shader source
  triangle.frag        # GLSL fragment shader source
  triangle.vert.spv    # compiled SPIR-V (keep source + SPIR-V here)
  triangle.frag.spv
Window.hpp / Window.cpp            # GLFW window (Vulkan, no OpenGL), sizes, cursor
Rectangle.hpp / Rectangle.cpp      # rectangle widget + click callback
VulkanRenderer.hpp / VulkanRenderer.cpp  # Vulkan device/swapchain/pipeline/batching
API_Portal.hpp                     # public API entry point
main.cpp                           # demo: rectangles + click callbacks
CMakeLists.txt                     # Vulkan + GLFW build
README.md                          # this file
```

The previous OpenGL implementation files remain on disk but are **not** part of
the active CMake build (OpenGL/GLEW are no longer required dependencies). They
are retained for reference; see "Migration status" below.

## Prerequisites

- **Vulkan SDK** (loader + headers + `glslc`/`glslangValidator` shader
  compiler). On this machine it is provided locally at:

      /Users/beckett/VulkanSDK/1.4.350.1

- **GLFW 3.4** (with Vulkan support) — installed via Homebrew on this machine.
- **CMake 3.15+** and a C++23 compiler (clang/g++).

No packages are downloaded or installed by the build. The Vulkan SDK path must
be supplied to CMake (see below).

## Build

From the project root:

```bash
# Configure (point CMake at the Vulkan SDK)
cmake -S . -B build -DVulkan_ROOT=/Users/beckett/VulkanSDK/1.4.350.1/macOS

# Build
cmake --build build
```

The executable is placed in `build/x19`.

## Run (from the project root, so `Shaders/` is found)

```bash
export VULKAN_SDK=/Users/beckett/VulkanSDK/1.4.350.1
export DYLD_LIBRARY_PATH="$VULKAN_SDK/macOS/lib:${DYLD_LIBRARY_PATH:-}"
export VK_ICD_FILENAMES="$VULKAN_SDK/macOS/share/vulkan/icd.d/MoltenVK_icd.json"

./build/x19
```

`VK_ICD_FILENAMES` is required on macOS so the Vulkan loader finds the MoltenVK
ICD. `DYLD_LIBRARY_PATH` makes `libvulkan.dylib` / `libMoltenVK.dylib`
discoverable.

You should see a window with three colored rectangles (red, green, blue).
Clicking inside a rectangle prints its callback to the console; clicking
outside prints nothing. The blue rectangle overlaps the red one and is
topmost, so clicks in the overlap hit the blue rectangle.

## Migration status

| Feature | Status |
|---|---|
| Vulkan window + render loop | ✅ Migrated |
| Filled rectangle (top-left origin) | ✅ Migrated |
| Rectangle click callbacks | ✅ Migrated |
| Correct hit-testing after resize / hi-DPI | ✅ Migrated |
| Topmost-first click overlap rule | ✅ Migrated |
| Single indexed batch draw for many rects | ✅ Migrated |
| Clean Vulkan/GLFW shutdown | ✅ Migrated |
| Text rendering (FreeType) | ⏳ Deferred (not part of first slice) |
| Borders / advanced styling | ⏳ Deferred |
| Texture arrays | ⏳ Deferred |
| Multi-window / layout system / multithreading | ⏳ Deferred |
| Legacy OpenGL sources | ⏳ Superseded (retained on disk, not built) |

Text, fonts, borders, advanced styling, layout systems, and multithreading are
deliberately out of scope for the first slice per the implementation
instructions. Existing OpenGL text/border/texture-array code was inspected for
intent but not preserved as a specification; those features are listed as
deferred and are not claimed to work.

## Known limitations

- Runtime requires a Vulkan ICD discoverable via `VK_ICD_FILENAMES` (MoltenVK
  on macOS) and `DYLD_LIBRARY_PATH` pointing at the SDK `lib` directory.
- A graphical display/window server is needed to actually open a window; if
  none is available the program builds but cannot be run here.
- The batch is bounded to 64 rectangles (fixed buffer sizing) for the first
  slice; raise `MAX_RECTANGLES` if more are needed.
- Rectangles are non-owning pointers held by the renderer; the caller must keep
  them alive while registered and remove them before destroying the renderer.

## Verification performed

- CMake configures and the project builds using already-available dependencies
  (no install/download).
- Shaders pre-compiled to SPIR-V with `glslc` from the local Vulkan SDK.
- Vulkan SDK verified functional via `vulkaninfo` (MoltenVK, Apple M3).
