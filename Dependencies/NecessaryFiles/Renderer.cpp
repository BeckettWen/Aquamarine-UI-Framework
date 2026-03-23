
#include "Renderer.hpp"

AquamarineRenderer::AquamarineRenderer(){
    BatchRenderingArray = std::make_unique<std::vector<float>>(std::vector<float>());
    BatchRenderingIndices = std::make_unique<std::vector<unsigned int>>(std::vector<unsigned int>());
    BatchRenderingArray_color = std::make_unique<std::vector<float>>(std::vector<float>());

    //this line below is also the test code and should be removed in the later development
    textEntity_Renderer = std::make_unique<std::vector<std::shared_ptr<AquamarineText>>>(
        std::vector<std::shared_ptr<AquamarineText>>());
    (*textEntity_Renderer).clear();
    (*textEntity_Renderer).emplace_back(std::make_shared<AquamarineText>());

    buttonTextMap = std::make_shared<std::map<AquamarineText*, std::string>>(std::map<AquamarineText*, std::string>());

    shaderHandlerEntity_Renderer = std::make_unique<ShaderHandler>();

    BatchRenderingArray_Color_UV = std::make_unique<std::vector<float>>(std::vector<float>());
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
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, (*BatchRenderingArray).size() * sizeof(float), (*BatchRenderingArray).data(), GL_STATIC_DRAW);

    glGenBuffers(1, &vertexIndiciesObject);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexIndiciesObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (*BatchRenderingIndices).size() * sizeof(unsigned int), 
            (*BatchRenderingIndices).data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
    //here is where the main before-render process ends

    //this is the generation of the array buffer of the button color
    glGenBuffers(1, &textureForButton_ColorBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textureForButton_ColorBuffer);
    glBufferData(GL_ARRAY_BUFFER, (*BatchRenderingArray_Color_UV).size() * sizeof(float),
            (*BatchRenderingArray_Color_UV).data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    //here is where the batch rendering texture will be in
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &textureForButton);
    glBindTexture(GL_TEXTURE_2D ,textureForButton);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D ,GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!BatchRenderingArray_color->empty()) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, BatchRenderingArray_color->data());
    } else {
        // Fallback: Create a single white pixel so the shader has SOMETHING to read
        float whitePixel[] = { 0.0f, 0.0f, 0.0f , 1.0f};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, whitePixel);
    }
    //glGenerateMipmap(GL_TEXTURE_2D);
    //here is the end of the texture input of the button or the triangle

    windowEntity_Renderer = std::make_shared<AquamarineWindow>(mainWindowEntity);

    (*shaderHandlerEntity_Renderer).CompileAndAttachShader();

    //this is the beginning  of the loading process of the texture of the button
    (*shaderHandlerEntity_Renderer).UseTraditionalShaderProgram();
    unsigned int textureForButton_Location = glGetUniformLocation((*shaderHandlerEntity_Renderer).shaderID_Program, "buttonTextureSampler");
    glUniform1i(textureForButton_Location, 0);
    unsigned int LocationOfOrthoGraphicProjection = glGetUniformLocation((*shaderHandlerEntity_Renderer).shaderID_Program, "OrthographicProjection");
    glm::mat4 orthoGraphicProjection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
    glUniformMatrix4fv(LocationOfOrthoGraphicProjection, 1, 0, &orthoGraphicProjection[0][0]);
    //this is the end of the loading process of the texture of the button

    //basically here is the source of the segmentation fault
    for(std::shared_ptr<AquamarineText> item: (*textEntity_Renderer)){
        (*item).SetupText(std::string(FontPath));
    }

    (*shaderHandlerEntity_Renderer).UseTextShaderProgram();
    //this is the orthographic projection matrix of the text, fot the button, you need to create another model projection matrix
    float AquamarineWindowWidth = 800;
    float AquamarineWindowHeight = 600;
    glm::mat4 orthoGraphic = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
    unsigned int AquamarineLocation = glGetUniformLocation((*shaderHandlerEntity_Renderer).shaderID_text_Program, "OrthographicProjection");
    glUniformMatrix4fv(AquamarineLocation, 1, 0, &orthoGraphic[0][0]);

    //this line calculates the count of the rendering of the triangles
    int renderCount = std::pow(2, -1)*(*BatchRenderingArray).size();
    std::string foundedText = "";

    while(!glfwWindowShouldClose((*(windowEntity_Renderer->windowEntity)))){
        //render the color onto the screen with the default black color
        glClearColor((*(windowEntity_Renderer->WindowColor))[0], 
            (*(windowEntity_Renderer->WindowColor))[1], 
            (*(windowEntity_Renderer->WindowColor))[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

        //here will be the button drawing process, using the indices to draw that triangle
        (*shaderHandlerEntity_Renderer).UseTraditionalShaderProgram();
        //this line draws all the button outline with the shape
        glBindVertexArray(vertexArrayObject);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureForButton);
        glDrawElements(GL_TRIANGLES, (*BatchRenderingIndices).size(), GL_UNSIGNED_INT, 0);

        //this is the test code of the text rendering, should be removed in the later process
        //for the changing of the color, you just need a vec3 of the color array to do that
        (*shaderHandlerEntity_Renderer).UseTextShaderProgram();
        for(std::shared_ptr<AquamarineText> item: (*textEntity_Renderer)){
            auto it = (*buttonTextMap).find(item.get());
            if(it != (*buttonTextMap).end()) {
                foundedText = (*it).second;
                (*item).RenderText(foundedText, 300.0f ,300.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (*shaderHandlerEntity_Renderer));
            }
            else{continue;}
        }

        glfwPollEvents();
        glfwSwapBuffers((*(windowEntity_Renderer->windowEntity)));
    }
    glfwTerminate();
}

template<typename T>
void AquamarineRenderer::AddWidget(T& widgetToBeAdded){}

template<> void AquamarineRenderer::AddWidget<AquamarineButton>(AquamarineButton& buttonWidget){
    if(ButtonHeaderExists == 1){
        //push the position of the button to the batch rendering process
        for(float item : (*(buttonWidget.ButtonPosition))){
            (*BatchRenderingArray).emplace_back(item);
        }
        (*BatchRenderingArray).shrink_to_fit();
    }
    else{
        std::cerr<<"You Need to Include the Button Header File first";
    }

    //this line defines the addition of the text of the button
    //probably the optimization later with the multi thread in cpp
    //so many things that happens now so i will put the optimization behind, at least after the alpha 1 is released
    (*textEntity_Renderer).emplace_back(buttonWidget.ButtonText);
    (*buttonTextMap).insert({buttonWidget.ButtonText.get(), buttonWidget.InternalText});

    for (float item : (*buttonWidget.ButtonColor)) {
        (*BatchRenderingArray_color).emplace_back(item);
    }

    for (float item : (*buttonWidget.ButtonUVCoords)) {
        (*BatchRenderingArray_Color_UV).emplace_back(item);
    }

    unsigned int autoIncrement = (*BatchRenderingArray).size()/2 - 4;

    for (unsigned int item : (*buttonWidget.ButtonPosition_Indicies)) {
        (*BatchRenderingIndices).emplace_back(item + autoIncrement);
    }
}
