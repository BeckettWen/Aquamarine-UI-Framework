
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <memory>

class AquamarineButton{
    protected:
    std::shared_ptr<std::array<float, 4>> ButtonColor;
    std::shared_ptr<std::array<float, 2>> ButtonSize;

    unsigned int AquamarineUI_Button_vertexBufferObject;
    unsigned int AquamarineUI_Button_vertexArrayObject;

    std::unique_ptr<std::array<float, 8>> ButtonPosition;

    AquamarineButton();
    ~AquamarineButton();

    void ChangeButtonSize( float width, float height );
    void AssignColorToButton( std::array<float, 4> userinputColor );
};