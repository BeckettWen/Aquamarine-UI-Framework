#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

class ShaderHandler{
    private:
    std::shared_ptr<std::string> shaderSource_vertex, shaderSource_fragment, shaderFilePath, shaderSource_textFragment;
    std::shared_ptr<std::stringstream> shaderSource_midwayHolder;

    std::shared_ptr<std::ifstream> fileStream;

    unsigned int shaderID_vertex, shaderID_Fragment, shaderID_textFragment, shaderID_Program, shaderID_text_Program;

    public:
    ShaderHandler();
    ~ShaderHandler();

    void CompileAndAttachShader();

    void UseTraditionalShaderProgram();
    void UseTextShaderProgram();

    friend class AquamarineText;
};