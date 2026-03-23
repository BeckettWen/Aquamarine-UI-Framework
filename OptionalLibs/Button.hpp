
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <memory>

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

    std::shared_ptr<AquamarineText> ButtonText;

    std::string InternalText;
    float actualAnchorPositionX, actualAnchorPositionY;

    public:
    AquamarineButton();
    ~AquamarineButton();

    void SetButtonPosition(float position_x, float position_y);
    void ChangeButtonSize( float width, float height );
    void AssignColorToButton( std::array<float, 4> userinputColor );
    void SetButtonText(std::string text);

    friend class AquamarineRenderer;
};