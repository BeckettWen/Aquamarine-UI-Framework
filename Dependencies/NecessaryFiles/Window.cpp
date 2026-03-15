
#include "Window.hpp"

AquamarineWindow::AquamarineWindow(){
    windowEntity = std::make_shared<GLFWwindow*>(nullptr);
    (*WindowColor) = {0.0f, 0.0f, 0.0f, 1.0f};
}

AquamarineWindow::~AquamarineWindow(){}

void AquamarineWindow::CreateDefaultWindow(std::string WindowTitle){
    *windowEntity = glfwCreateWindow(800, 600, WindowTitle.c_str(), NULL, NULL);
    glfwMakeContextCurrent(*windowEntity);
}

void AquamarineWindow::SetWindowColor(float color_r, float color_g, float color_b){
    (*WindowColor) = {color_r, color_g, color_b};
}

std::shared_ptr<GLFWwindow*> AquamarineWindow::GetTheWindowEntity(){
    return windowEntity;
}