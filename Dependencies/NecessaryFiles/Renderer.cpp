
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

    BatchRenderingArray_TextureIndex = std::make_unique<std::vector<float>>(std::vector<float>());
    buttonTextScaleRatioMap = std::make_shared<std::map<AquamarineText*, float>>(std::map<AquamarineText*, float>());

    textPositionMap = std::make_shared<std::map<AquamarineText*, std::array<float, 2>>>(std::map<AquamarineText*, std::array<float, 2>>());

    textSizeMap = std::make_shared<std::map<AquamarineText*, std::array<float,2>>>(std::map<AquamarineText*, std::array<float, 2>>());

    buttonWidgetarray = std::make_shared<std::vector<AquamarineButton*>>(std::vector<AquamarineButton*>());

    borderVertexPosition = std::make_shared<std::vector<float>>(std::vector<float>());
    borderVertexIndices = std::make_shared<std::vector<unsigned int>>(std::vector<unsigned int>());

    seperateText = std::make_shared<std::vector<AquamarineText>>(std::vector<AquamarineText>());
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

    //here is where the rendering process of the batch rendering process, this process is where the batch rendering texture happens
    glGenBuffers(1, &textureIndexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, textureIndexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (*BatchRenderingArray_TextureIndex).size() * sizeof(float),
        (*BatchRenderingArray_TextureIndex).data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, 0 );

    int numOfButtons = currenttexturelayer;
    glGenTextures(1, &textArrayObject);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textArrayObject);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA,
    1, 1,        // width, height (1x1 solid color per button)
    numOfButtons,  // depth = number of buttons
    0, GL_RGBA, GL_FLOAT, nullptr);

    // Upload each button's color to its own layer
    for(int i = 0; i < numOfButtons; i++){
        float* colorData = (*BatchRenderingArray_color).data() + (i * 4); // 4 floats per button
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
            0, 0, i,   // x, y, layer index
            1, 1, 1,   // width, height, depth
            GL_RGBA, GL_FLOAT, colorData);
    }

    //these generation of the single texture will be abandoned in the later process

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

    //insert the draw process of the border line here
    glGenVertexArrays(1, &borderVertexArrayObject);
    glBindVertexArray(borderVertexArrayObject);

    glGenBuffers(1, &borderVertexBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, borderVertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, (*borderVertexPosition).size() * sizeof(float), (*borderVertexPosition).data(), GL_STATIC_DRAW);

    glGenBuffers(1, &borderVertexPositionIndices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, borderVertexPositionIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (*borderVertexIndices).size() * sizeof(unsigned int), (*borderVertexIndices).data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    //notice that you still need to push the array into the array in the renderer to actually draw the border of the button


    //basically here is the source of the segmentation fault
    for(std::shared_ptr<AquamarineText> item: (*textEntity_Renderer)){
        (*item).SetupText(std::string(FontPath));


        auto textIt = (*buttonTextMap).find(item.get());
        auto scaleIt = (*buttonTextScaleRatioMap).find(item.get());
        if(textIt != (*buttonTextMap).end() && scaleIt != (*buttonTextScaleRatioMap).end()){
            float labelWidth = 0.0f, labelHeight = 0.0f;
            float scaleRatio = scaleIt->second;
            for(char c : textIt->second){
                auto charIt = (*item).textCharacters.find(c);
                if(charIt != (*item).textCharacters.end()){
                    labelWidth += (charIt->second.text_advance >> 6) * scaleRatio;
                    labelHeight = std::max(labelHeight, (float)charIt->second.text_Bearing.y * scaleRatio);
                }
            }
            (*textSizeMap)[item.get()] = std::array<float, 2>({labelWidth, labelHeight});
        }

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

    float actualStartPositionX, actualStartPositionY;

    //this is the main loop of setting the callback function in the renderer
    glfwSetWindowUserPointer(*(windowEntity_Renderer->windowEntity), buttonWidgetarray.get());
    glfwSetMouseButtonCallback(*(windowEntity_Renderer->windowEntity),
        [](GLFWwindow* windowEntity, int button, int action, int mods){
            // cast to vector, not a single button
            auto* list = static_cast<std::vector<AquamarineButton*>*>(glfwGetWindowUserPointer(windowEntity));
            if(action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT){
                double mouseX, mouseY;
                glfwGetCursorPos(windowEntity, &mouseX, &mouseY);
                mouseY = 600.0f - mouseY;
                for(auto& btn : *list){
                    if((btn->actualAnchorPositionX - (*(btn->ButtonSize))[0]*0.5f) < mouseX &&
                       (btn->actualAnchorPositionX + (*(btn->ButtonSize))[0]*0.5f) > mouseX &&
                       (btn->actualAnchorPositionY - (*(btn->ButtonSize))[1]*0.5f) < mouseY &&
                       (btn->actualAnchorPositionY + (*(btn->ButtonSize))[1]*0.5f) > mouseY){
                        for(auto& fn : (*btn->ButtonResponseFunctions)){
                            fn();
                        }
                    }
                }
            }
        });

    while(!glfwWindowShouldClose((*(windowEntity_Renderer->windowEntity)))){
        //render the color onto the screen with the default black color
        glClearColor((*(windowEntity_Renderer->WindowColor))[0], 
            (*(windowEntity_Renderer->WindowColor))[1], 
            (*(windowEntity_Renderer->WindowColor))[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        //you should draw the borderlines here to avoid the mis bound of the vertexarray object
        glBindVertexArray(borderVertexArrayObject);
        glDrawElements(GL_TRIANGLES, (*borderVertexIndices).size(), GL_UNSIGNED_INT, 0 );


        //here will be the button drawing process, using the indices to draw that triangle
        (*shaderHandlerEntity_Renderer).UseTraditionalShaderProgram();
        //this line draws all the button outline with the shape
        glBindVertexArray(vertexArrayObject);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textArrayObject);
        glDrawElements(GL_TRIANGLES, (*BatchRenderingIndices).size(), GL_UNSIGNED_INT, 0);

        //this is the test code of the text rendering, should be removed in the later process
        //for the changing of the color, you just need a vec3 of the color array to do that
        (*shaderHandlerEntity_Renderer).UseTextShaderProgram();
        for(std::shared_ptr<AquamarineText> item: (*textEntity_Renderer)){
            auto it = (*buttonTextMap).find(item.get());
            auto it2 = (*textPositionMap).find(item.get());
            auto it3 = (*buttonTextScaleRatioMap).find(item.get());
            auto it4 = (*textSizeMap).find(item.get());
            if(it != (*buttonTextMap).end() && it2 != (*textPositionMap).end() && it3 != (*buttonTextScaleRatioMap).end() && it4 != (*textPositionMap).end()) {
                actualStartPositionX = it2->second[0] - 0.5f*(it4->second[0]);
                actualStartPositionY = it2->second[1] - 0.5f*(it4->second[1]);
                foundedText = (*it).second;
                (*item).RenderText(foundedText, actualStartPositionX ,actualStartPositionY, it3->second, glm::vec3(1.0f, 1.0f, 1.0f), (*shaderHandlerEntity_Renderer));
            }
            else{continue;}
        }

        //here is the button callback function

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
    (*textPositionMap).insert({buttonWidget.ButtonText.get(), std::array<float, 2>({buttonWidget.actualAnchorPositionX, buttonWidget.actualAnchorPositionY})});

    for (float item : (*buttonWidget.ButtonColor)) {
        (*BatchRenderingArray_color).emplace_back(item);
    }
    //i know, i know, you say that this could be optimized with the multithreading, i have no time to do this in alpha 1 ok?
    for (float item : (*buttonWidget.ButtonUVCoords)) {
        (*BatchRenderingArray_Color_UV).emplace_back(item);
    }

    unsigned int autoIncrement = (*BatchRenderingArray).size()/2 - 4;

    for (unsigned int item : (*buttonWidget.ButtonPosition_Indicies)) {
        (*BatchRenderingIndices).emplace_back(item + autoIncrement);
    }

    //here adds the texture layer of the different buttons
    float layerIndex = static_cast<float>(currenttexturelayer);
    (*buttonWidget.ButtonTextureLayer) = std::array<float, 4>({
        layerIndex, layerIndex, layerIndex, layerIndex
    });
    currenttexturelayer++;

    for(float item : (*buttonWidget.ButtonTextureLayer)){
        (*BatchRenderingArray_TextureIndex).emplace_back(item);
    }
    //here is the texture layer process ends

    //here calculates the scale ratio of the text inside the button
    //here use the standard 800 600 config of the window, and you should later change this to the unified window width and height
    float scaleRatio = std::min(std::pow(800.0f, -1)*(*buttonWidget.ButtonSize)[0], std::pow(600.0f, -1)*(*buttonWidget.ButtonSize)[1]);
    (*buttonTextScaleRatioMap).insert({buttonWidget.ButtonText.get(), scaleRatio});

    //here stores the button size for the center layout of the text in the button
    float labelWidth = 0.0f;
    float labelHeight = 0.0f;
    for(char c : buttonWidget.InternalText){
        auto charIt = (*buttonWidget.ButtonText).textCharacters.find(c);
        if(charIt != (*buttonWidget.ButtonText).textCharacters.end()){
            labelWidth  += (charIt->second.text_advance >> 6) * scaleRatio;
            labelHeight  = std::max(labelHeight, (float)charIt->second.text_Bearing.y * scaleRatio);
        }
    }
    (*textSizeMap).insert({buttonWidget.ButtonText.get(),
        std::array<float, 2>({labelWidth, labelHeight})});

    (*buttonWidgetarray).emplace_back(&buttonWidget);
}
