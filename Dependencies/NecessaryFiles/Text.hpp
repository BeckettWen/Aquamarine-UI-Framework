#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <format>

#include "ShaderHandling.hpp"

struct AquamarineTextCharacter{
    unsigned int text_textureID;
    glm::ivec2 text_Size;
    glm::ivec2 text_Bearing;
    unsigned int text_advance;
};


class AquamarineText{
    protected:
    std::map<char, AquamarineTextCharacter> textCharacters;
    unsigned int textVAO, textVBO;
    std::weak_ptr<ShaderHandler> shaderHandler_Pointer;
    float generalTextSize = 0.0f, textXSize = 0.0f, textYSize = 0.0f, anchorPointPositionX, anchorPointPositionY;//this defines the width and height of the text for the later definition of the anchor point

    public:
    AquamarineText();
    ~AquamarineText();

    void SetupText(std::string fontPath);
    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color, ShaderHandler& shaderHandler);

    friend class AquamarineRenderer;
};
