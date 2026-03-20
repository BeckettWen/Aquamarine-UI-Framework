cmake_minimum_required(VERSION 3.15)
project(AquamarineUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CONFIGURE_DEPENDS "CMakeLists.txt")

# 1. Set C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Find Necessary Packages
# Find OpenGL (Internal to CMake)
find_package(OpenGL REQUIRED)

# Find GLEW (Internal to CMake)
# For static linking, set: set(GLEW_USE_STATIC_LIBS TRUE)
find_package(GLEW REQUIRED)

# Find GLFW (Installed via Homebrew)
# Some systems use 'glfw3', others use 'GLFW' or 'glfw'
find_package(glfw3 CONFIG REQUIRED)

find_package(Freetype REQUIRED)

# 3. Add Executable
# Replace main.cpp with your actual list of source files
add_executable(${PROJECT_NAME} 
    main.cpp
    API_Portal.hpp
    ./Dependencies/NecessaryFiles/ShaderHandling.hpp
    ./Dependencies/NecessaryFiles/ShaderHandling.cpp
    ./Dependencies/NecessaryFiles/Window.hpp
    ./Dependencies/NecessaryFiles/Window.cpp
    ./Dependencies/NecessaryFiles/Renderer.hpp
    ./Dependencies/NecessaryFiles/Renderer.cpp
    ./OptionalLibs/Button.hpp
    ./OptionalLibs/Button.cpp
    ./Dependencies/NecessaryFiles/general_include.hpp
    ./Dependencies/NecessaryFiles/Text.hpp
    ./Dependencies/NecessaryFiles/Text.cpp
)

# 4. Link Libraries using Modern Targets
# Target-based linking handles include directories automatically
target_link_libraries(${PROJECT_NAME} PRIVATE 
    OpenGL::GL 
    GLEW::GLEW 
    glfw
    Freetype::Freetype
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/"
    FontPath="${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/fonts/HelveticaNeueUltraLight.ttf"
)

# 5. Platform-Specific Fixes (For macOS Frameworks)
# GLFW on macOS often requires extra system frameworks to link correctly
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        "-framework Cocoa" 
        "-framework IOKit" 
        "-framework CoreVideo"
    )
endif()

