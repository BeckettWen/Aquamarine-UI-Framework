
#include "ShaderHandling.hpp"

std::ostream& operator<<(std::ostream& stream, std::vector<char>& errorMessage){
    for (const char character : errorMessage){
       stream << character; 
    }
    return stream;
}

ShaderHandler::ShaderHandler() :shaderID_Fragment(0), shaderID_vertex(0) {

    shaderSource_midwayHolder = std::make_unique<std::stringstream>();
    shaderSource_vertex = std::make_shared<std::string>();
    shaderSource_fragment = std::make_shared<std::string>();

    fileStream = std::make_unique<std::ifstream>();
    
    shaderFilePath = std::make_shared<std::string>(
        static_cast<std::string>(SHADER_PATH) + "Dependencies/Shaders"
    );
}

ShaderHandler::~ShaderHandler(){}

void ShaderHandler::CompileAndAttachShader(){
    shaderID_vertex = glCreateShader(GL_VERTEX_SHADER);
    shaderID_Fragment = glCreateShader(GL_FRAGMENT_SHADER);

    int result = GL_FALSE;
    int infolength;

    (*fileStream).clear();
    (*fileStream).open((*shaderFilePath)+"vertexShader.glsl", std::ios::in);

    if((*fileStream).is_open()){
        (*shaderSource_midwayHolder) << (*fileStream).rdbuf();
        *shaderSource_vertex = (*shaderSource_midwayHolder).str();
        (*shaderSource_midwayHolder).clear();
        (*fileStream).close();
    }
    else{ std::cerr<<"Shader File Not Opened (vertex)\n"; }
    (*fileStream).clear();

    char const * shaderVertexString = (*shaderSource_vertex).c_str();
    glShaderSource(shaderID_vertex, 1, &shaderVertexString, NULL);
    glCompileShader(shaderID_vertex);

    (*fileStream).open((*shaderFilePath)+"fragmentShader.glsl", std::ios::in);

    if((*fileStream).is_open()){
        (*shaderSource_midwayHolder) << (*fileStream).rdbuf();
        *shaderSource_fragment = (*shaderSource_midwayHolder).str();
        (*shaderSource_midwayHolder).clear();
        (*fileStream).close();
    }
    else{ std::cerr<<"Shader File Not Opened (fragment)\n";}

    char const * shaderFragmentString = (*shaderSource_fragment).c_str();
    glShaderSource(shaderID_Fragment, 1, &shaderFragmentString, NULL);
    glCompileShader(shaderID_Fragment);
    
    shaderID_Program = glCreateProgram();
    glAttachShader(shaderID_Program, shaderID_vertex);
}