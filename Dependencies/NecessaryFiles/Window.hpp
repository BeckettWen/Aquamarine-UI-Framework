#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <functional>
#include <iostream>
#include <vector>
#include <memory>


class AquamarineWindow{
    public:
    std::shared_ptr<GLFWwindow*> windowEntity;
protected:
    std::shared_ptr<std::array<float,4>> WindowColor;
    
    AquamarineWindow();
    public:
    ~AquamarineWindow();

    public:
    void CreateDefaultWindow(std::string WindowTitle = "Default Window");
    void SetWindowColor(float color_r, float color_g, float color_b);

    std::shared_ptr<GLFWwindow*> GetTheWindowEntity();

    friend class AquamarineRenderer;
};