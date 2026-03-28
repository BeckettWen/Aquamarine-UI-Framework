
#include "Button.hpp"

AquamarineButton::AquamarineButton(): 
    ButtonColor(std::make_shared<std::array<float,4>>(std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f}))),
    ButtonSize(std::make_shared<std::array<float,2>>(std::array<float,2>({80.0f, 45.0f}))){
    ButtonUVCoords = std::make_shared<std::array<float, 8>>(std::array<float, 8>());
    ButtonPosition = std::make_unique<std::array<float, 8>>(std::array<float, 8>());
    ButtonPosition_Indicies = std::make_unique<std::array<unsigned int, 6>>(std::array<unsigned int, 6>({0,1,2,0,3,2}));

    ButtonTextureLayer = std::make_shared<std::array<float, 4>>(std::array<float, 4>({0.0f, 0.0f, 0.0f, 0.0f}));

    ButtonResponseFunctions = std::make_shared<std::vector<std::function<void()>>>(std::vector<std::function<void()>>());
    ButtonResponseFunctionsMap = std::make_shared<std::map<std::function<void()>*, int>>(std::map<std::function<void()>*, int>());
    }

AquamarineButton::~AquamarineButton(){}

void AquamarineButton::ChangeButtonSize( float width, float height ){
    ButtonSize = std::make_shared<std::array<float, 2>>(std::array<float,2>({width, height}));
}

void AquamarineButton::SetButtonPosition(float position_x, float position_y){
    //these lines define the assisting position locator of the button text
    actualAnchorPositionX = position_x;
    actualAnchorPositionY = position_y;

    ButtonPosition = std::make_unique<std::array<float, 8>>(std::array<float, 8>({
        (position_x - 0.5f*(*ButtonSize)[0]), position_y + 0.5f*(*ButtonSize)[1],
        position_x - 0.5f*(*ButtonSize)[0], position_y - 0.5f*(*ButtonSize)[1],
        position_x + 0.5f*(*ButtonSize)[0], position_y - 0.5f*(*ButtonSize)[1],
        position_x + 0.5f*(*ButtonSize)[0], position_y + 0.5f*(*ButtonSize)[1]
    }));
}

void AquamarineButton::SetButtonText(std::string text) {
    ButtonText = std::make_shared<AquamarineText>();
    InternalText = text;
}

void AquamarineButton::AssignColorToButton(std::array<float, 4> userinputColor) {
    (*ButtonColor) = userinputColor;
    (*ButtonUVCoords) = std::array<float, 8>({0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f});
}



void AquamarineButton::SetButtonResponseFunction(std::function<void()> responseFunction, triggertype responseCondition, GLFWwindow* windowEntity) {
    (*ButtonResponseFunctions).emplace_back(responseFunction);
    //(*ButtonResponseFunctionsMap).insert({&responseFunction, responseCondition});
    //here is the end of the button function callback
}
