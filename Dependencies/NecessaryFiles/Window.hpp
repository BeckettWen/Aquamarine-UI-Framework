#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <functional>
#include <iostream>
#include <vector>
#include <memory>

#include "Renderer.hpp"

class AquamarineWindow{
    protected:
    std::shared_ptr<GLFWwindow*> windowEntity;
    std::shared_ptr<std::array<float,4>> WindowColor;
    
    AquamarineWindow();
    ~AquamarineWindow();

    void CreateDefaultWindow(std::string WindowTitle = "Default Window");
    void SetWindowColor(float color_r, float color_g, float color_b);

    std::shared_ptr<GLFWwindow*> GetTheWindowEntity();

    friend class AquamarineRenderer;
};