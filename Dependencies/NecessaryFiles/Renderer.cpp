
#include "Renderer.hpp"

AquamarineRenderer::AquamarineRenderer(){
    BatchRenderingArray = std::make_unique<std::vector<float>>();
    BatchRenderingIndices = std::make_unique<std::vector<unsigned int>>();
    BatchRenderingArray_color = std::make_unique<std::vector<float>>();
}
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

void AquamarineRenderer::StartMainRenderLoop(AquamarineWindow& mainWindowEntity){
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

    //here is where the batch rendering texture will be in
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &textureForButton);
    glBindTexture(GL_TEXTURE_2D ,textureForButton);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D ,GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //don't use this line until the texture is set, or the program will throw the segmentation fault
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_FLOAT, (*BatchRenderingArray_color).data());

    glGenerateMipmap(GL_TEXTURE_2D);
    //here is the end of the texture input of the button or the triangle

    windowEntity_Renderer = std::make_shared<AquamarineWindow>(mainWindowEntity);

    while(!glfwWindowShouldClose((*(windowEntity_Renderer->windowEntity)))){
        //render the color onto the screen with the default black color
        glClearColor((*(windowEntity_Renderer->WindowColor))[0], 
            (*(windowEntity_Renderer->WindowColor))[1], 
            (*(windowEntity_Renderer->WindowColor))[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

        glfwPollEvents();
        glfwSwapBuffers((*(windowEntity_Renderer->windowEntity)));
    }
    glfwTerminate();
}

template<typename T>
void AquamarineRenderer::AddWidget(T& widgetToBeAdded){}

template<>
void AquamarineRenderer::AddWidget<AquamarineButton>(AquamarineButton& buttonWidget){
    if(ButtonHeaderExists == 1){
        //push the position of the button to the batch rendering process
        for(float item : (*(buttonWidget.ButtonPosition))){
            (*BatchRenderingArray).emplace_back(item);
        }
        (*BatchRenderingArray).shrink_to_fit();
    }
    else{
        std::cerr<<"You need to include the button header file First";
    }
}