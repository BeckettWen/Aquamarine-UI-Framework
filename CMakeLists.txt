cmake_minimum_required(VERSION 3.15)
project(X19 VERSION 0.2 LANGUAGES CXX)

# X-19 Lyra (Cloudsdale) — Vulkan-based UI framework with Phase 2 texture engine.
# Active renderer: Vulkan only (no OpenGL / GLEW in the active build).

set(CMAKE_CONFIGURE_DEPENDS "CMakeLists.txt")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
# Vulkan loader + headers. Provide -DVulkan_ROOT=<VulkanSDK>/macOS on the
# cmake command line, or set VULKAN_SDK in the environment.
find_package(Vulkan REQUIRED)

# GLFW (Homebrew). GLFW 3.4 has Vulkan support (glfwCreateWindowSurface etc.).
find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)

# Prefer the installed Helvetica Neue collection, selecting its Regular face.
# Override this cache variable with a regular/semibold TTF, OTF, or TTC path on
# another machine. The font is referenced in place and is not copied/bundled.
set(LYRA_FONT_FILE "/System/Library/Fonts/SFNS.ttf" CACHE FILEPATH
    "Path to a regular or semibold Lyra UI font")
if(NOT EXISTS "${LYRA_FONT_FILE}")
    message(FATAL_ERROR
        "Font not found: ${LYRA_FONT_FILE}. Configure with -DLYRA_FONT_FILE=/path/to/a/regular-or-semibold-font.ttf")
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(x19
    main.cpp
    API_Portal.hpp
    Window.hpp
    Window.cpp
    Rectangle.hpp
    Rectangle.cpp
    Text.hpp
    Text.cpp
    Texture.hpp
    Texture.cpp
    VulkanRenderer.hpp
    VulkanRenderer.cpp
)

target_include_directories(x19 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(x19 PRIVATE Vulkan::Vulkan glfw Freetype::Freetype)

# Shader directory (source + compiled .spv files are loaded at runtime from here).
target_compile_definitions(x19 PRIVATE
    SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
)

# Assets directory (PPM P6 textures loaded at runtime from here).
target_compile_definitions(x19 PRIVATE
    ASSET_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
)

target_compile_definitions(x19 PRIVATE
    FONT_PATH="${LYRA_FONT_FILE}"
)

# macOS frameworks needed by GLFW / Vulkan (MoltenVK -> Metal).
if(APPLE)
    target_link_libraries(x19 PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework QuartzCore"
        "-framework Metal"
    )
endif()

# Keep build artifacts inside the project (under build/).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(CTest)
if(BUILD_TESTING)
    add_executable(text_atlas_test
        Text.cpp
        tests/TextAtlasTest.cpp
    )
    target_include_directories(text_atlas_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(text_atlas_test PRIVATE Freetype::Freetype)
    target_compile_definitions(text_atlas_test PRIVATE
        TEST_FONT_PATH="${LYRA_FONT_FILE}"
    )
    add_test(NAME text_atlas_smoke COMMAND text_atlas_test)
endif()
