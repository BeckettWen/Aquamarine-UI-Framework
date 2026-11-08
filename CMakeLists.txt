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
    Texture.hpp
    Texture.cpp
    VulkanRenderer.hpp
    VulkanRenderer.cpp
)

target_include_directories(x19 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(x19 PRIVATE Vulkan::Vulkan glfw)

# Shader directory (source + compiled .spv files are loaded at runtime from here).
target_compile_definitions(x19 PRIVATE
    SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
)

# Assets directory (PPM P6 textures loaded at runtime from here).
target_compile_definitions(x19 PRIVATE
    ASSET_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
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
