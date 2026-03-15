
#include "Renderer.hpp"

AquamarineRenderer::AquamarineRenderer(){}
AquamarineRenderer::~AquamarineRenderer(){}

void AquamarineRenderer::InitialGLEW(){
    int result = glewInit();
    if(result != GLEW_OK){
        std::cerr<<"GLEW initialization failed\n";
        InitializationResult = false;
    }
}

void AquamarineRenderer::InitialGLFW(){
    bool result = glfwInit();
    if(!result){
        std::cerr<<"GLFW initialization failed\n";
        InitializationResult = false;
    }
    //necessary for the window rendering process
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
}

void AquamarineRenderer::StartMainRenderLoop(){
    //here is where the main before-render process starts
    glGenVertexArrays(1, &vertexArrayObject);
    glBindVertexArray(vertexArrayObject);

    glGenBuffers(1, &vertexBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexArrayObject);
    glBufferData(GL_ARRAY_BUFFER, (*BatchRenderingArray).size() * sizeof(float), (*BatchRenderingArray).data(), GL_STATIC_DRAW);

    glGenBuffers(1, &vertexIndiciesObject);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexIndiciesObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (*BatchRenderingIndices).size() * sizeof(unsigned int), 
            (*BatchRenderingIndices).data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
    //here is where the main before-render process ends


    while(glfwWindowShouldClose((*(windowEntity_Renderer->windowEntity)))){
        //render the color onto the screen with the default black color
        glClearColor((*(windowEntity_Renderer->WindowColor))[0], 
            (*(windowEntity_Renderer->WindowColor))[1], 
            (*(windowEntity_Renderer->WindowColor))[2], 1.0f);

        glfwPollEvents();
        glfwSwapBuffers((*(windowEntity_Renderer->windowEntity)));
    }
    glfwTerminate();
}