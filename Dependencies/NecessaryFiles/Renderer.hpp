
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <memory>
#include <thread>

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
    std::unique_ptr<std::vector<float>> BatchRenderingArray_Color_UV;
    std::unique_ptr<ShaderHandler> shaderHandlerEntity_Renderer;

    std::shared_ptr<std::map<AquamarineText*, std::string>> buttonTextMap;
    std::shared_ptr<std::map<AquamarineText*, float>> buttonTextScaleRatioMap;

    std::shared_ptr<std::map<AquamarineText*, std::array<float, 2>>> textSizeMap;
    std::shared_ptr<std::map<AquamarineText*, std::array<float, 2>>> textPositionMap;

    std::shared_ptr<std::vector<AquamarineButton*>> buttonWidgetarray;

    std::shared_ptr<std::vector<AquamarineText>> seperateText;

    unsigned int vertexBufferObject;
    unsigned int vertexArrayObject;
    unsigned int vertexIndiciesObject;
    unsigned int textureForButton_ColorBuffer;

    //this is where the border of the button is drawn
    unsigned int borderVertexArrayObject;
    unsigned int borderVertexBufferObject;
    unsigned int borderVertexPositionIndices;
    std::shared_ptr<std::vector<float>> borderVertexPosition;
    std::shared_ptr<std::vector<unsigned int>> borderVertexIndices;

    //these are the variables that needed in the process of the batch rendering of the textures
    unsigned int currenttexturelayer = 0;
    std::unique_ptr<std::vector<float>> BatchRenderingArray_TextureIndex;
    unsigned int textureIndexBuffer;
    unsigned int textArrayObject;

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