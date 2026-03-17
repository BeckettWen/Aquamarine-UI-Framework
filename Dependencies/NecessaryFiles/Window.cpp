
#include "Window.hpp"

AquamarineWindow::AquamarineWindow(){
    windowEntity = std::make_shared<GLFWwindow*>(nullptr);
    WindowColor = std::make_shared<std::array<float, 4>>(std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f}));
}

AquamarineWindow::~AquamarineWindow(){}

void AquamarineWindow::CreateDefaultWindow(std::string WindowTitle){
    *windowEntity = glfwCreateWindow(800, 600, WindowTitle.c_str(), NULL, NULL);
    glfwMakeContextCurrent(*windowEntity);
}

void AquamarineWindow::SetWindowColor(float color_r, float color_g, float color_b){
    WindowColor = std::make_shared<std::array<float,4>>(std::array<float, 4>({color_r, color_g, color_b, 1.0f}));
}

std::shared_ptr<GLFWwindow*> AquamarineWindow::GetTheWindowEntity(){
    return windowEntity;
}