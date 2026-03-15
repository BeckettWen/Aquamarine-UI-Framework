
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include "Window.hpp"

#if __has_include("../../OptionalLibs/Button.hpp")
    #include "../../OptionalLibs/Button.hpp"
    #define headerExists 1
#else
    #define headerExists 0
#endif

class AquamarineRenderer{
    protected:

    bool InitializationResult = true;
    std::shared_ptr<AquamarineWindow> windowEntity_Renderer;

    std::unique_ptr<std::vector<float>> BatchRenderingArray;
    std::unique_ptr<std::vector<unsigned int>> BatchRenderingIndices;

    std::unique_ptr<std::vector<float>> BatchRenderingArray_color;

    unsigned int vertexBufferObject;
    unsigned int vertexArrayObject;
    unsigned int vertexIndiciesObject;

    unsigned int textureForButton;

    AquamarineRenderer();
    ~AquamarineRenderer();

    void InitialGLEW();
    void InitialGLFW();

    void StartMainRenderLoop();

    template<typename T> void AddWidget(T& WidgetObject);
    template<> void AddWidget<AquamarineButton>(AquamarineButton& buttonObject);

    friend class ShaderHandler;
    friend class AquamarineWindow;
};