# X-19 Lyra (Cloudsdale) — Phase 2 Texture Engine

Vulkan-based experimental UI framework with a texture engine.
This is **Phase 2** of Lyra (first version: Cloudsdale). Phase 1 implemented
the Vulkan foundation + rectangle/click vertical slice. Phase 2 adds runtime
texture loading, GPU upload into a Vulkan 2D-array image, and sampled
texturing in the batch renderer.

**Backend: Vulkan** (MoltenVK on macOS). OpenGL/GLEW are not part of the
active build.

## Implemented

### Phase 1 (rectangle + click, retained)
- GLFW window configured for Vulkan (no OpenGL context).
- Vulkan instance, surface, physical/logical device, graphics+present queue,
  swapchain, image views, graphics pipeline, command resources, synchronization,
  orderly teardown.
- Visible filled rectangles (top-left-origin position/size convention).
- Rectangle widgets with click callbacks.
- Pointer hit-testing using actual window/framebuffer dimensions, with
  resize handling and GLFW top-left-origin cursor coordinates.
- Deterministic overlap rule: latest-added / topmost first.
- Batching: rectangles grouped by texture layer, one indexed batch draw per
  compatible group.
- Callback semantics: fires once per click inside a rectangle; not for
  outside clicks, cursor motion, or unrelated rectangles.
- Clean shutdown on window close.

### Phase 2 (texture engine, newly added)
- **Texture API:** `TextureRecord` + `TextureManager`; renderer-owned texture
  collection with stable layer indices; `addTexture()` returns a layer index.
- **Image input:** PPM P6 (binary) self-contained loader, no external
  dependencies. Validates dimensions, channel count, byte count, file
  open/read results, and overflow before allocating/uploading.
- **Vulkan upload:** staging buffer, `VkImage` (`VK_IMAGE_VIEW_TYPE_2D_ARRAY`),
  image view, sampler, layout transitions (UNDEFINED → TRANSFER_DST →
  SHADER_READ_ONLY), transfer-to-fragment synchronization.
- **Descriptors:** descriptor set layout, pool, set with a combined image
  sampler binding for the texture array.
- **Batch geometry:** vertices extended with UV coordinates (`vec2`) and the
  texture layer is passed via push constants (`int`, -1 = solid color).
- **Shaders:** `textured.vert` / `textured.frag` (GLSL source + compiled
  SPIR.V in `Shaders/`).
- **Solid-color path:** preserved via `textureLayer = -1` (no texture sample,
  vertex color output directly).
- **Demo:** solid and textured rectangles, multiple array layers proven by
  rendering different images on different rectangles.

## Repository layout

```
Shaders/
  textured.vert        # GLSL vertex shader source (Phase 2)
  textured.frag        # GLSL fragment shader source (Phase 2)
  textured.vert.spv    # compiled SPIR-V
  textured.frag.spv
  triangle.vert / .frag / .spv   # Phase 1 shaders (retained, unused)
Assets/
  checkerboard.ppm     # test texture asset (64x64 RGB PPM P6)
  gradient.ppm         # test texture asset (64x64 RGB PPM P6)
Texture.hpp / Texture.cpp      # PPM loader + TextureManager (Vulkan array resources)
Window.hpp / Window.cpp        # GLFW window (Vulkan, no OpenGL)
Rectangle.hpp / Rectangle.cpp  # rectangle widget + UVs + textureLayer + click
VulkanRenderer.hpp / VulkanRenderer.cpp  # Vulkan device/swapchain/pipeline/descriptors/batching
API_Portal.hpp                 # public API entry point
main.cpp                       # demo: solid + textured rectangles + click callbacks
CMakeLists.txt                 # Vulkan + GLFW build
README.md                      # this file
```

Legacy OpenGL sources under `Dependencies/` and `OptionalLibs/` remain on disk
but are **not** part of the active CMake build.

## Prerequisites

- **Vulkan SDK** (loader + headers + `glslc` shader compiler). On this machine
  it is provided locally at:

      /Users/beckett/VulkanSDK/1.4.350.1

- **GLFW 3.4** (with Vulkan support) — installed via Homebrew on this machine.
- **CMake 3.15+** and a C++23 compiler (clang/g++).

No packages are downloaded or installed by the build. The Vulkan SDK path must
be supplied to CMake (see below).

## Build

From the project root:

```bash
cmake -S . -B build -DVulkan_ROOT=/Users/beckett/VulkanSDK/1.4.350.1/macOS
cmake --build build
```

The executable is placed in `build/x19`.

## Run (from the project root, so `Shaders/` and `Assets/` are found)

```bash
./run.sh
```

`run.sh` sets `VULKAN_SDK`, `DYLD_LIBRARY_PATH`, and `VK_ICD_FILENAMES`
automatically (needed for MoltenVK on macOS) and launches `build/x19`.

You should see a window with solid-color and textured rectangles. Clicking
inside a rectangle prints its callback to the console; clicking outside prints
nothing. The gradient rectangle overlaps the checkerboard and is topmost, so
clicks in the overlap hit the gradient rectangle.

## Migration status

| Feature | Status |
|---|---|
| Vulkan window + render loop | ✅ Implemented |
| Solid-color rectangle (top-left origin) | ✅ Implemented |
| Rectangle click callbacks | ✅ Implemented |
| Hit-testing after resize / hi-DPI | ✅ Implemented |
| Topmost-first click overlap | ✅ Implemented |
| Batched indexed draws (per texture group) | ✅ Implemented |
| Clean Vulkan/GLFW shutdown | ✅ Implemented |
| Texture loading (PPM P6) | ✅ Implemented (Phase 2) |
| Vulkan 2D-array texture upload | ✅ Implemented (Phase 2) |
| Texture sampling in fragment shader | ✅ Implemented (Phase 2) |
| Multiple texture layers | ✅ Implemented (Phase 2) |
| Text rendering (FreeType) | ⏳ Deferred |
| Borders / advanced styling | ⏳ Deferred |
| PNG/JPEG decoding | ⏳ Deferred (PPM P6 only) |
| Multi-window / layout system / multithreading | ⏳ Deferred |
| Legacy OpenGL sources | ⏳ Superseded (retained on disk, not built) |

## Supported texture format

- **PPM P6 (binary):** `P6\nwidth height\n255\n<RGB bytes>`
  - RGB (3 channels) or RGBA (4 channels)
  - Max value must be 255
  - Dimensions must match across all textures in the same array
  - All textures in one array share width/height/channel count

## Draw-call behavior

Rectangles are grouped by `textureLayer`. Solid rectangles (`layer = -1`)
form one group; each textured layer forms its own group. Each group issues
one indexed draw call. Observed draw calls = number of distinct texture
layers present + 1 (solid group). If texture grouping would reorder
overlapping transparent widgets, insertion order within each group preserves
the painter's order.

## Known limitations

- Runtime requires `run.sh` (or equivalent env vars) for MoltenVK ICD
  discovery on macOS.
- Validation layers are present in the SDK but not auto-loaded; run with
  `VK_LAYER_PATH` pointing at the SDK's explicit_layer.d directory if
  desired.
- A graphical display/window server is needed to open a window.
- Batch is bounded to 64 rectangles (`MAX_RECTANGLES`).
- Textures are non-owning in the renderer; the TextureManager owns Vulkan
  resources and destroys them in valid order.

## Verification performed

- CMake configures and builds using already-available dependencies.
- Shaders compiled to SPIR-V with `glslc` from the local Vulkan SDK.
- Vulkan SDK verified functional via `vulkaninfo` (MoltenVK, Apple M3).
- Demo runs and displays textured rectangles from runtime-loaded PPM assets.
- Solid-color rectangles still render; click callbacks, resize, and clean
  shutdown verified on Phase 1 path.
