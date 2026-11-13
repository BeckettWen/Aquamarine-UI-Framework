cmake_minimum_required(VERSION 3.15)

# Version.hpp is the single source of truth for package metadata and runtime
# version reporting. Fail during configure if it is missing/incomplete.
set(LYRA_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Version.hpp")
if(NOT EXISTS "${LYRA_VERSION_HEADER}")
    message(FATAL_ERROR "Required version header is missing: ${LYRA_VERSION_HEADER}")
endif()
file(READ "${LYRA_VERSION_HEADER}" LYRA_VERSION_HEADER_CONTENT)
foreach(component MAJOR MINOR PATCH)
    string(REGEX MATCH
        "#define LYRA_VERSION_${component} +([0-9]+)"
        version_match "${LYRA_VERSION_HEADER_CONTENT}")
    if(NOT version_match)
        message(FATAL_ERROR
            "Version.hpp must define numeric LYRA_VERSION_${component}")
    endif()
    set(LYRA_VERSION_${component} "${CMAKE_MATCH_1}")
endforeach()

project(Lyra VERSION "${LYRA_VERSION_MAJOR}.${LYRA_VERSION_MINOR}.${LYRA_VERSION_PATCH}"
        LANGUAGES CXX)

option(LYRA_ENABLE_BACKDROP_BLUR
    "Build the in-app Gaussian backdrop blur for widgets and text" ON)
option(LYRA_ENABLE_MACOS_WINDOW_BLUR
    "Build the optional macOS NSVisualEffectView window backdrop" OFF)

if(LYRA_ENABLE_MACOS_WINDOW_BLUR)
    if(NOT APPLE)
        message(FATAL_ERROR "LYRA_ENABLE_MACOS_WINDOW_BLUR is only supported on macOS")
    endif()
    if(NOT CMAKE_GENERATOR STREQUAL "Xcode" AND
       NOT CMAKE_GENERATOR MATCHES "Ninja")
        message(FATAL_ERROR
            "The optional Swift/AppKit blur layer requires the Xcode or Ninja generator. "
            "Configure with -G Xcode (or -G Ninja).")
    endif()
    enable_language(OBJCXX)
    enable_language(Swift)
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${LYRA_VERSION_HEADER}")

# Lyra (Cloudsdale) — Vulkan-based UI framework with texturing and animation.
# Active renderer: Vulkan only (no OpenGL / GLEW in the active build).

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

# Prefer the installed SF Pro font, selecting its Regular face.
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
    Version.hpp
    main.cpp
    API_Portal.hpp
    Window.hpp
    Window.cpp
    Animation.hpp
    Animation.cpp
    Rectangle.hpp
    Rectangle.cpp
    Text.hpp
    Text.cpp
    Texture.hpp
    Texture.cpp
    VulkanRenderer.hpp
    VulkanRenderer.cpp
)

if(LYRA_ENABLE_MACOS_WINDOW_BLUR)
    target_sources(x19 PRIVATE MacOSWindowBlur.mm MacOSWindowBlur.swift)
    target_compile_definitions(x19 PRIVATE LYRA_ENABLE_MACOS_WINDOW_BLUR=1)
    target_link_libraries(x19 PRIVATE "-framework AppKit")
endif()

if(LYRA_ENABLE_BACKDROP_BLUR)
    target_compile_definitions(x19 PRIVATE LYRA_ENABLE_BACKDROP_BLUR=1)
endif()

if(LYRA_ENABLE_BACKDROP_BLUR OR LYRA_ENABLE_MACOS_WINDOW_BLUR)
    target_compile_definitions(x19 PRIVATE LYRA_ENABLE_OFFSCREEN_RENDERING=1)

    find_program(LYRA_GLSLC_EXECUTABLE glslc
        HINTS "${Vulkan_ROOT}/bin" "$ENV{VULKAN_SDK}/bin")
    if(NOT LYRA_GLSLC_EXECUTABLE)
        message(FATAL_ERROR
            "Backdrop rendering requires glslc from the Vulkan SDK. Set Vulkan_ROOT or VULKAN_SDK.")
    endif()

    set(LYRA_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
    file(MAKE_DIRECTORY "${LYRA_SHADER_OUTPUT_DIR}")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/"
         DESTINATION "${LYRA_SHADER_OUTPUT_DIR}")
    set(LYRA_BACKDROP_SHADER_OUTPUTS)
    foreach(shader backdrop_blur.vert backdrop_blur.frag)
        set(shader_output "${LYRA_SHADER_OUTPUT_DIR}/${shader}.spv")
        add_custom_command(
            OUTPUT "${shader_output}"
            COMMAND "${LYRA_GLSLC_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${shader}"
                    -o "${shader_output}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${shader}"
            VERBATIM
        )
        list(APPEND LYRA_BACKDROP_SHADER_OUTPUTS "${shader_output}")
    endforeach()
    add_custom_target(lyra_backdrop_shaders DEPENDS ${LYRA_BACKDROP_SHADER_OUTPUTS})
    add_dependencies(x19 lyra_backdrop_shaders)
else()
    set(LYRA_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
endif()

target_include_directories(x19 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(x19 PRIVATE Vulkan::Vulkan glfw Freetype::Freetype)

# Shader directory (source + compiled .spv files are loaded at runtime from here).
target_compile_definitions(x19 PRIVATE
    SHADER_PATH="${LYRA_SHADER_OUTPUT_DIR}"
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
        Version.hpp
        Text.cpp
        tests/TextAtlasTest.cpp
    )
    target_include_directories(text_atlas_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(text_atlas_test PRIVATE Freetype::Freetype)
    target_compile_definitions(text_atlas_test PRIVATE
        TEST_FONT_PATH="${LYRA_FONT_FILE}"
    )
    add_test(NAME text_atlas_smoke COMMAND text_atlas_test)

    add_executable(mouse_dispatch_test
        Version.hpp
        Animation.cpp
        VulkanRenderer.cpp
        Texture.cpp
        Text.cpp
        Rectangle.cpp
        tests/MouseDispatchTest.cpp
    )
    target_include_directories(mouse_dispatch_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(mouse_dispatch_test PRIVATE
        Vulkan::Vulkan glfw Freetype::Freetype)
    add_test(NAME mouse_dispatch COMMAND mouse_dispatch_test)

    add_executable(animation_test
        Version.hpp
        Animation.cpp
        tests/AnimationTest.cpp
    )
    target_include_directories(animation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME animation_presets COMMAND animation_test)
endif()
