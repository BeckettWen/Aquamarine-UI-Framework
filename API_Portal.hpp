
//this is the general API Portal of this UI Framework

#pragma once

#include "Dependencies/NecessaryFiles/ShaderHandling.hpp"
#include "Dependencies/NecessaryFiles/Window.hpp"
#include "Dependencies/NecessaryFiles/Renderer.hpp"
#include "Dependencies/NecessaryFiles/general_include.hpp"

class AquamarineUI_Window : public AquamarineWindow{};

class AquamarineUI_Shader : public ShaderHandler{};

class AquamarineUI_Renderer: public AquamarineRenderer{};

class AquamarineUI_Button: public AquamarineButton{};