
#include "Text.hpp"

AquamarineText::AquamarineText(){}
AquamarineText::~AquamarineText(){}

void AquamarineText::SetupText(std::string fontPath){
    FT_Library fontLibrary;
    try{
        FT_Init_FreeType(&fontLibrary);
    }catch(std::exception& exception){
        std::cerr<<"Font Library Load Failed\n";
    }

    FT_Face fontFace;
    // 2. Check Face Loading (THIS IS THE CRITICAL CHECK)
    if (FT_New_Face(fontLibrary, fontPath.c_str(), 0, &fontFace)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font at: " << fontPath << std::endl;
        FT_Done_FreeType(fontLibrary); // Clean up library before exiting
        return; // Stop here so we don't crash in the loop below
    }

    FT_Set_Pixel_Sizes(fontFace, 0, 64);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    unsigned int textTextureID;

    //GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
    //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

    for(unsigned char chara = 0; chara<128;chara++){
        if(FT_Load_Char(fontFace, chara, FT_LOAD_RENDER)){continue;}

        glGenTextures(1, &textTextureID);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textTextureID);// Red for RGB, 1.0 for Alpha
        //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, fontFace->glyph->bitmap.width, fontFace->glyph->bitmap.rows, 
            0, GL_RED, GL_UNSIGNED_BYTE, fontFace->glyph->bitmap.buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        AquamarineTextCharacter character = {
            textTextureID, 
            glm::ivec2(fontFace->glyph->bitmap.width, fontFace->glyph->bitmap.rows),
            glm::ivec2(fontFace->glyph->bitmap_left, fontFace->glyph->bitmap_top),
            static_cast<unsigned int>(fontFace->glyph->advance.x)
        };
        //this line is where the bug happens
        textCharacters.insert(std::pair<char, AquamarineTextCharacter>(chara, character));

        textXSize += character.text_Size.x;
        textYSize += character.text_Size.y;
    }
    
    FT_Done_Face(fontFace);
    FT_Done_FreeType(fontLibrary);

    //the normal generation process of the vao and vbo
    glGenVertexArrays(1, &textVAO);
    glBindVertexArray(textVAO);

    glGenBuffers(1, &textVBO);
    glBindBuffer(GL_ARRAY_BUFFER ,textVBO);
    glBufferData(GL_ARRAY_BUFFER, 6*4*sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
}

void AquamarineText::RenderText(std::string text, float x, float y, float scale, glm::vec3 color, ShaderHandler& shaderHandler){
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //you need to use the program that is bound to the text fragment shader
    glUniform3f(glGetUniformLocation(shaderHandler.shaderID_text_Program, "textColor"), 
        color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE2);
    glBindVertexArray(textVAO);

    for (std::string::const_iterator c = text.begin(); c != text.end(); c++) {
        AquamarineTextCharacter ch = textCharacters[*c];

        float xpos = x + ch.text_Bearing.x * scale;
        float ypos = y - (ch.text_Size.y - ch.text_Bearing.y) * scale;

        float w = ch.text_Size.x * scale;
        float h = ch.text_Size.y * scale;

        // Update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },            
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }           
        };

        // Render glyph texture over quad
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ch.text_textureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); 
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Advance cursor for next glyph (advance is number of 1/64 pixels)
        x += (ch.text_advance >> 6) * scale; 
    }
    //glBindVertexArray(0);
    //glBindTexture(GL_TEXTURE_2D, 0);
}