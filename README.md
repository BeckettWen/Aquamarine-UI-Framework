# Lyra

### First version: Cloudsdale

Vulkan-based experimental UI framework. Phase 1 implemented the Vulkan
foundation and rectangle/click vertical slice. Phase 2 adds runtime texture
loading and text rendered through independent image and glyph texture arrays.
The animation design adds position animation presets, repeat counts, modified-
batch detection, and mouse event dispatch.

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
- Batching: contiguous rectangles with the same texture layer share an indexed
  draw while preserving painter's order.
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
  SPIR-V in `Shaders/`).
- **Solid-color path:** preserved via `textureLayer = -1` (no texture sample,
  vertex color output directly).
- **Demo:** solid and textured rectangles, multiple array layers proven by
  rendering different images on different rectangles.
- **Text rendering:** `TextLabel` text is rasterized from SF Pro with
  FreeType and rendered from a separate glyph texture array, descriptor set,
  graphics pipeline, and vertex/index batch. Text is drawn after rectangles.
- **Font weight:** the default system font is
  `/System/Library/Fonts/SFNS.ttf`; FreeType selects its Regular face
  (or SemiBold if Regular is unavailable). Set `LYRA_FONT_FILE` at CMake
  configure time to use another Regular/SemiBold TTF, OTF, or TTC. The system
  font is referenced in place and is not copied into the repository.
- **Glyph data:** printable ASCII glyphs are stored in padded `R8_UNORM` array
  layers; text quads carry their own UV and glyph-layer data. Spaces advance
  the baseline without drawing a quad.
- **Text layout:** label `x` and `baselineY` use top-left-origin logical window
  pixels. Scale `1.0` corresponds to the 48-pixel glyph rasterization size.

### Animation and input
- Rectangles support linear position animation and cubic Bezier paths with two
  control points.
- Each animation accepts a total play count; `0` means repeat indefinitely.
- The renderer advances animations once per frame and compares the current
  rectangle batch inputs with the previous snapshot. When geometry or related
  batch data changes, it rebuilds the mapped vertex/index arrays before drawing.
- GLFW mouse-button events are captured by `Window` and dispatched to the
  topmost enabled rectangle under the cursor. A left-button press invokes its
  `onClick` response once; other buttons/actions and outside clicks are ignored.
- The demo animates the red rectangle with the linear preset and the green
  rectangle with a repeating cubic Bezier path.

## Repository layout

```
Shaders/
  textured.vert        # GLSL vertex shader source (Phase 2)
  textured.frag        # GLSL fragment shader source (Phase 2)
  textured.vert.spv    # compiled SPIR-V
  textured.frag.spv
  text.vert / text.frag           # dedicated glyph-array shaders
  text.vert.spv / text.frag.spv
  triangle.vert / .frag / .spv   # Phase 1 shaders (retained, unused)
Assets/
  checkerboard.ppm     # test texture asset (64x64 RGB PPM P6)
  gradient.ppm         # test texture asset (64x64 RGB PPM P6)
Texture.hpp / Texture.cpp      # PPM loader + TextureManager (Vulkan array resources)
Text.hpp / Text.cpp            # FreeType glyph rasterizer + TextLabel API
Animation.hpp / Animation.cpp  # linear and cubic Bezier position animation
Window.hpp / Window.cpp        # GLFW window (Vulkan, no OpenGL)
Rectangle.hpp / Rectangle.cpp  # rectangle widget + UVs + textureLayer + click
VulkanRenderer.hpp / VulkanRenderer.cpp  # Vulkan device/swapchain/pipeline/descriptors/batching
API_Portal.hpp                 # public API entry point
main.cpp                       # demo: textures, text, animation, and click callbacks
CMakeLists.txt                 # Vulkan + GLFW build
README.md                      # this file
tests/TextAtlasTest.cpp        # CPU-side glyph atlas smoke test
tests/AnimationTest.cpp        # preset and repeat-count tests
tests/MouseDispatchTest.cpp    # topmost click dispatch test
```

Legacy OpenGL sources under `Dependencies/` and `OptionalLibs/` remain on disk
but are **not** part of the active CMake build.

## Prerequisites

- **Vulkan SDK** (loader + headers + `glslc` shader compiler). On this machine
  it is provided locally at:

      /Users/beckett/VulkanSDK/1.4.350.1

- **GLFW 3.4** (with Vulkan support) — installed via Homebrew on this machine.
- **FreeType** — required for glyph rasterization; CMake uses an already
  available installation (`find_package(Freetype REQUIRED)`). No package is
  installed by the project.
- A Regular or SemiBold font face. On macOS the default is the installed
  SF Pro system font; override it with `-DLYRA_FONT_FILE=/path/to/font`
  when configuring CMake on another system.
- **CMake 3.15+** and a C++23 compiler (clang/g++).

No packages are downloaded or installed by the build. The Vulkan SDK path must
be supplied to CMake (see below).

## Build

From the project root:

```bash
cmake -S . -B build -DVulkan_ROOT=/Users/beckett/VulkanSDK/1.4.350.1/macOS
cmake --build build
ctest --test-dir build --output-on-failure
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
clicks in the overlap hit the gradient rectangle. The red and green rectangles
also animate using the linear and Bezier presets.

## Migration status

| Feature | Status |
|---|---|
| Vulkan window + render loop | ✅ Implemented |
| Solid-color rectangle (top-left origin) | ✅ Implemented |
| Rectangle click callbacks | ✅ Implemented |
| Hit-testing after resize / hi-DPI | ✅ Implemented |
| Topmost-first click overlap | ✅ Implemented |
| Batched indexed draws (contiguous texture-layer runs) | ✅ Implemented |
| Clean Vulkan/GLFW shutdown | ✅ Implemented |
| Texture loading (PPM P6) | ✅ Implemented (Phase 2) |
| Vulkan 2D-array texture upload | ✅ Implemented (Phase 2) |
| Texture sampling in fragment shader | ✅ Implemented (Phase 2) |
| Multiple texture layers | ✅ Implemented (Phase 2) |
| Text rendering (FreeType; printable ASCII) | ✅ Implemented (separate glyph array and batch) |
| Linear / cubic Bezier rectangle animation | ✅ Implemented |
| Per-animation repeat count (`0` = infinite) | ✅ Implemented |
| Batch-input modification detection and rebuild | ✅ Implemented |
| GLFW mouse capture and rectangle click dispatch | ✅ Implemented |
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

## Text rendering

Create a label with its text, logical-pixel origin/baseline, scale, and RGBA
color, then register it with `VulkanRenderer::addText()`. Keep registered
labels alive while the renderer uses them. The demo shows the API in `main.cpp`.

The default `/System/Library/Fonts/SFNS.ttf` (SF Pro) is rasterized by FreeType
at 48 pixels, selecting Regular first and SemiBold as a fallback. Printable
ASCII is supported; other bytes use the `?` fallback. Newline and tab are
supported. UTF-8 shaping, kerning, and advanced typography are not implemented.
Text uses its own padded one-layer-per-glyph `R8_UNORM` Vulkan 2D array,
descriptor set, pipeline, and mapped geometry buffers; it never shares the
UI-image texture array or rectangle buffers. Glyph bitmap rows and screen
coordinates both use a top-left origin, so the text is upright. All visible
text is submitted as one indexed text draw per frame (up to 4096 glyph quads),
after the rectangle draw groups.

## Draw-call behavior

Rectangles are combined into contiguous draw groups with the same texture
layer. Grouping only adjacent rectangles preserves painter's order and
transparency. The draw-call count is the number of contiguous texture-layer
runs in the visible rectangle list; modifying rectangle batch data triggers a
batch rebuild before the next draw.

## Known limitations

- Runtime requires `run.sh` (or equivalent env vars) for MoltenVK ICD
  discovery on macOS.
- Validation layers are present in the SDK but not auto-loaded; run with
  `VK_LAYER_PATH` pointing at the SDK's explicit_layer.d directory if
  desired.
- A graphical display/window server is needed to open a window.
- Batch is bounded to 64 rectangles (`MAX_RECTANGLES`).
- Text batch is bounded to 4096 visible glyph quads (`MAX_TEXT_GLYPHS`).
- Text rasterization requires an existing FreeType installation and a Regular
  or SemiBold font face; it supports printable ASCII, newline, and tab only.
- Textures are non-owning in the renderer; the TextureManager owns Vulkan
  resources and destroys them in valid order.
- Bezier animations use explicit control points; repeat count `0` is infinite.
  Finite repeats restart from the animation's starting position.

## Verification performed

- CMake configures and builds using already-available dependencies.
- Shaders compiled to SPIR-V with `glslc` from the local Vulkan SDK.
- Vulkan SDK verified functional via `vulkaninfo` (MoltenVK, Apple M3).
- Demo runs and displays textured rectangles from runtime-loaded PPM assets.
- Solid-color rectangles still render; click callbacks, resize, and clean
  shutdown verified on Phase 1 path.
- Text atlas smoke test passes; the Vulkan demo initializes and loads the
  Regular font glyph array. On-screen glyph orientation was corrected in the
  vertex shader; visual inspection is still recommended.
- Animation preset/repeat and mouse-dispatch tests pass. The interactive demo
  starts with the animation and input paths enabled.
