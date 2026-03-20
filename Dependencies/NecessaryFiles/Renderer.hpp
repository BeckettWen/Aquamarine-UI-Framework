
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <memory>

#include "Text.hpp"
#include "Window.hpp"
#include "general_include.hpp"

class AquamarineRenderer{
    protected:

    bool InitializationResult = true;
    std::shared_ptr<AquamarineWindow> windowEntity_Renderer;
    std::unique_ptr<std::vector<std::shared_ptr<AquamarineText>>> textEntity_Renderer;

    std::unique_ptr<std::vector<float>> BatchRenderingArray;
    std::unique_ptr<std::vector<unsigned int>> BatchRenderingIndices;

    std::unique_ptr<std::vector<float>> BatchRenderingArray_color;

    std::unique_ptr<ShaderHandler> shaderHandlerEntity_Renderer;

    unsigned int vertexBufferObject;
    unsigned int vertexArrayObject;
    unsigned int vertexIndiciesObject;

    unsigned int textureForButton;

    AquamarineRenderer();
    public:
    ~AquamarineRenderer();

    public:
    void InitialGLEW();
    void InitialGLFW();

    void StartMainRenderLoop(AquamarineWindow& mainWindowEntity);

    template<typename T> void AddWidget(T& WidgetObject);
    template<> void AddWidget<AquamarineButton>(AquamarineButton& buttonObject);

    friend class ShaderHandler;
    friend class AquamarineWindow;
};