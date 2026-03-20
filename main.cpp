
//this is the example of the UI Framework, DO NOT COPY THIS TO YOUR MAIN APPLICATION
//Notice this UI Framework is still in Experimental Phase

#include "API_Portal.hpp"
#include "Dependencies/NecessaryFiles/ShaderHandling.hpp"

int main(){
   std::unique_ptr<AquamarineUI_Renderer> Renderer = std::make_unique<AquamarineUI_Renderer>();
   (*Renderer).InitialGLFW();
   
   std::unique_ptr<AquamarineUI_Window> mainWindow = std::make_unique<AquamarineUI_Window>();
    (*mainWindow).CreateDefaultWindow("Unit test Phase 1 Addition 2");
    (*Renderer).InitialGLEW();

    (*mainWindow).SetWindowColor(1.0f, 1.0f, 1.0f);
   (*Renderer).StartMainRenderLoop((*mainWindow));
}