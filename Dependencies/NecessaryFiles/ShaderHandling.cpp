
#include "ShaderHandling.hpp"

ShaderHandler::ShaderHandler(){

    shaderSource_midwayHolder = std::make_shared<std::stringstream>();
    shaderSource_vertex = std::make_shared<std::string>();
    shaderSource_fragment = std::make_shared<std::string>();

    fileStream = std::make_shared<std::ifstream>();
    
    shaderFilePath = std::make_shared<std::string>();
    (*shaderFilePath) = std::string(SHADER_PATH) + "Dependencies/Shaders/";

    std::cout<<"Object Creation successful\n";
}

ShaderHandler::~ShaderHandler(){
    glDetachShader(shaderID_Program, shaderID_vertex);
    glDetachShader(shaderID_Program, shaderID_Fragment);

    glDeleteShader(shaderID_vertex);
    glDeleteShader(shaderID_Fragment);

    std::cout<<"Object Destruction Successful\n";
}

std::ostream& operator<<(std::ostream& stream, std::vector<char>& errorMessage){
    for (const char character : errorMessage){
       stream << character; 
    }
    return stream;
}

void ShaderHandler::CompileAndAttachShader(){
    shaderID_vertex = glCreateShader(GL_VERTEX_SHADER);
    shaderID_Fragment = glCreateShader(GL_FRAGMENT_SHADER);
    shaderID_textFragment = glCreateShader(GL_FRAGMENT_SHADER);

    std::string shaderPath_vertex, shaderPath_Fragment, shaderPath_textFragment;
    shaderPath_vertex = std::string((*shaderFilePath)) + "vertexShader.glsl";
    shaderPath_Fragment = std::string((*shaderFilePath)) + "fragmentShader.glsl";
    shaderPath_textFragment = std::string((*shaderFilePath)) + "textFragmentShader.glsl";

    int result = GL_FALSE;
    int infolength;

    (*fileStream).open(shaderPath_vertex, std::ios::in);

    if((*fileStream).is_open()){
        (*shaderSource_midwayHolder) << (*fileStream).rdbuf();
        *shaderSource_vertex = (*shaderSource_midwayHolder).str();
        (*shaderSource_midwayHolder).str("");
        (*fileStream).close();
    }
    else{ std::cerr<<"Shader File Not Opened (vertex)\n"; }

    char const * shaderVertexString = (*shaderSource_vertex).c_str();
    glShaderSource(shaderID_vertex, 1, &shaderVertexString, NULL);
    glCompileShader(shaderID_vertex);

    glGetShaderiv(shaderID_vertex, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shaderID_vertex, GL_INFO_LOG_LENGTH, &infolength);
    if(infolength > 0){
        std::shared_ptr<std::vector<char>> infolog = std::make_shared<std::vector<char>>();
        (*infolog).resize(infolength+1);
        glGetShaderInfoLog(shaderID_vertex, (*infolog).size(), NULL, (*infolog).data());
        std::cout<<"shader vertex Compilation Error:"<<std::string((*infolog).begin(), (*infolog).end());
    }

    result = GL_FALSE;
    infolength = 0;

    (*fileStream).open(shaderPath_Fragment, std::ios::in);

    if((*fileStream).is_open()){
        (*shaderSource_midwayHolder) << (*fileStream).rdbuf();
        *shaderSource_fragment = (*shaderSource_midwayHolder).str();
        (*shaderSource_midwayHolder).str("");
        (*fileStream).close();
    }
    else{ std::cerr<<"Shader File Not Opened (fragment)\n";}

    char const * shaderFragmentString = (*shaderSource_fragment).c_str();
    glShaderSource(shaderID_Fragment, 1, &shaderFragmentString, NULL);
    glCompileShader(shaderID_Fragment);

    glGetShaderiv(shaderID_Fragment, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shaderID_Fragment, GL_INFO_LOG_LENGTH, &infolength);
    if(infolength > 0){
        std::shared_ptr<std::vector<char>> infolog = std::make_shared<std::vector<char>>();
        (*infolog).resize(infolength + 1);
        glGetShaderInfoLog(shaderID_Fragment, (*infolog).size(), NULL,(*infolog).data());
        std::cerr<<"shader Fragment compilation error:"<< std::string((*infolog).begin(), (*infolog).end());
    }

    result = GL_FALSE;
    infolength = 0;

    (*fileStream).open(shaderPath_textFragment, std::ios::in);
    if((*fileStream).is_open()){
        (*shaderSource_midwayHolder) << (*fileStream).rdbuf();
        (*shaderSource_textFragment) = (*shaderSource_midwayHolder).str();
        (*shaderSource_midwayHolder).str("");
        (*fileStream).close();
    }
    else{ std::cerr<<"Shader text fragment not opened\n";}

    const char * shadertextFragmentString = (*shaderSource_textFragment).c_str();
    glShaderSource(shaderID_textFragment, 1, &shadertextFragmentString, NULL);
    glCompileShader(shaderID_textFragment);

    glGetShaderiv(shaderID_textFragment, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shaderID_textFragment, GL_INFO_LOG_LENGTH, &infolength);
    if(infolength > 0){
        std::shared_ptr<std::vector<char>> infolog = std::make_shared<std::vector<char>>();
        (*infolog).resize(infolength + 1);
        glGetShaderInfoLog(shaderID_textFragment, (*infolog).size(), NULL,(*infolog).data());
        std::cerr<<"shader Fragment compilation error:"<< std::string((*infolog).begin(), (*infolog).end());
    }
    
    shaderID_Program = glCreateProgram();
    glAttachShader(shaderID_Program, shaderID_vertex);
    glAttachShader(shaderID_Program, shaderID_Fragment);

    shaderID_text_Program = glCreateProgram();
    glAttachShader(shaderID_text_Program, shaderID_vertex);
    glAttachShader(shaderID_text_Program, shaderID_textFragment);

    glLinkProgram(shaderID_Program);
    glValidateProgram(shaderID_Program);

    glLinkProgram(shaderID_text_Program);
    glValidateProgram(shaderID_text_Program);

    int linkResult = GL_FALSE;
    glGetProgramiv(shaderID_Program, GL_LINK_STATUS, &linkResult);
    glGetProgramiv(shaderID_Program, GL_INFO_LOG_LENGTH, &infolength);
    if(infolength > 0){
        std::shared_ptr<std::vector<char>> infoLog = std::make_shared<std::vector<char>>();
        (*infoLog).resize(infolength + 1);
        glGetProgramInfoLog(shaderID_Program, (*infoLog).size(), NULL, &(*infoLog)[0]);
        std::cerr<<"shader Fragment compilation error:"<<(*infoLog);
    }

    infolength = 0;
    linkResult = GL_FALSE;
    glGetProgramiv(shaderID_text_Program, GL_LINK_STATUS, &linkResult);
    glGetProgramiv(shaderID_text_Program, GL_INFO_LOG_LENGTH, &infolength);
    if(infolength > 0){
        std::shared_ptr<std::vector<char>> infoLog = std::make_shared<std::vector<char>>();
        (*infoLog).resize(infolength + 1);
        glGetProgramInfoLog(shaderID_text_Program, (*infoLog).size(), NULL, &(*infoLog)[0]);
        std::cerr<<"shader Fragment compilation error:"<<(*infoLog);
    }
}

void ShaderHandler::UseTraditionalShaderProgram(){ glUseProgram(shaderID_Program);}
void ShaderHandler::UseTextShaderProgram(){ glUseProgram(shaderID_text_Program);}