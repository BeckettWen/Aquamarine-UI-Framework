
//this is the example of the UI Framework, DO NOT COPY THIS TO YOUR MAIN APPLICATION
//Notice that this UI Framework is still in Experimental Phase

#include "API_Portal.hpp"
#include "Dependencies/NecessaryFiles/ShaderHandling.hpp"

int main(){
   std::unique_ptr<AquamarineUI_Renderer> Renderer = std::make_unique<AquamarineUI_Renderer>();
   (*Renderer).InitialGLFW();
   
   std::unique_ptr<AquamarineUI_Window> mainWindow = std::make_unique<AquamarineUI_Window>();
    (*mainWindow).CreateDefaultWindow("Aquamarine UI Alpha 1");
    (*Renderer).InitialGLEW();

    std::shared_ptr<AquamarineButton> button = std::make_shared<AquamarineButton>();
    (*button).ChangeButtonSize(600.0f, 200.0f);
    (*button).SetButtonPosition(400.0f, 500.0f);
    (*button).SetButtonText("Welcome");
    (*button).AssignColorToButton(std::array<float, 4>({1.0f, 1.0f, 1.0f, 1.0f}));
    (*button).SetButtonResponseFunction([](){std::cout<<"button1 clicked\n";}, AquamarineButton::triggertype::button_clicked, *(*mainWindow).windowEntity.get());

    std::shared_ptr<AquamarineButton> button2 = std::make_shared<AquamarineButton>();
    (*button2).ChangeButtonSize(600.0f, 100.0f);
    (*button2).SetButtonPosition(400.0f, 300.0f);
    (*button2).SetButtonText("Aquamarine UI Kit Alpha 1");
    (*button2).AssignColorToButton(std::array<float, 4>({0.7f, 0.7f, 0.7f, 0.5f}));
    (*button2).SetButtonResponseFunction([](){std::cout<<"button1 clicked\n";}, AquamarineButton::triggertype::button_clicked, *(*mainWindow).windowEntity.get());


    (*Renderer).AddWidget(*button);
    (*Renderer).AddWidget(*button2);

    (*mainWindow).SetWindowColor(1.0f, 1.0f, 1.0f);
   (*Renderer).StartMainRenderLoop((*mainWindow));
}