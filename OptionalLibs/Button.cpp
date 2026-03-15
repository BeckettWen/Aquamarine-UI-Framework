
#include "Button.hpp"

AquamarineButton::AquamarineButton(): 
    ButtonColor(std::make_shared<std::array<float,4>>(std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f}))),
    ButtonSize(std::make_shared<std::array<float,2>>(std::array<float,2>({80.0f, 45.0f}))){
        
        //this is the begining of the button rendering process
        //the vertex buffer object
        glGenVertexArrays( 1, &AquamarineUI_Button_vertexArrayObject );
        glBindVertexArray( AquamarineUI_Button_vertexArrayObject );

        //the vertex array object
        glGenBuffers( 1, &AquamarineUI_Button_vertexBufferObject );
        glBindBuffer( GL_ARRAY_BUFFER , AquamarineUI_Button_vertexBufferObject );

        ButtonPosition = std::make_unique<std::array<float, 8>>(std::array<float, 8>({}));
    }

AquamarineButton::~AquamarineButton(){}

void AquamarineButton::ChangeButtonSize( float width, float height ){
    ( *ButtonSize )[0] = width;
    (*ButtonSize)[1] = height;
}
