
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <memory>
#include <functional>

#include "../Dependencies/NecessaryFiles/Text.hpp"

class AquamarineButton{
    protected:
    std::shared_ptr<std::array<float, 4>> ButtonColor;
    std::shared_ptr<std::array<float, 8>> ButtonUVCoords;
    std::shared_ptr<std::array<float, 2>> ButtonSize;

    unsigned int AquamarineUI_Button_vertexBufferObject;
    unsigned int AquamarineUI_Button_vertexArrayObject;

    //this position represents the vertex position of the button triangle
    std::unique_ptr<std::array<float, 8>> ButtonPosition;
    std::unique_ptr<std::array<unsigned int, 6>> ButtonPosition_Indicies;

    std::shared_ptr<std::array<float, 4>> ButtonTextureLayer;

    std::shared_ptr<AquamarineText> ButtonText;

    std::shared_ptr<std::vector<std::function<void()>>> ButtonResponseFunctions;
    std::shared_ptr<std::map<std::function<void()>*, int>> ButtonResponseFunctionsMap;

    std::string InternalText;
    float actualAnchorPositionX, actualAnchorPositionY;

    float borderThickness = 0.0f;


    public:
    enum triggertype : int{
        button_clicked = GLFW_PRESS, button_long_pressed
    };

    AquamarineButton();
    ~AquamarineButton();

    void SetButtonPosition(float position_x, float position_y);
    void ChangeButtonSize( float width, float height );
    void AssignColorToButton( std::array<float, 4> userinputColor );
    void SetButtonText(std::string text);

    void SetButtonResponseFunction(std::function<void()> responseFunction, triggertype responseCondition, GLFWwindow* windowEntity);

    friend class AquamarineRenderer;
};