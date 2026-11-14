#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Text.hpp"
#include "Texture.hpp"

#include <chrono>
#include <vector>

class Rectangle;

// macOS renderer API adapter. Its Objective-C++ implementation bridges the
// shared Lyra widget/input model to the Swift/Metal rendering core.
class MetalRenderer {
public:
    MetalRenderer() = default;
    ~MetalRenderer();

    bool init(GLFWwindow* window, int fbWidth, int fbHeight,
              int winWidth, int winHeight, const char* unusedShaderDir = nullptr);
    void destroy();

    int addRectangle(Rectangle* rectangle);
    void removeRectangle(Rectangle* rectangle);
    int addTexture(TextureRecord texture);
    bool uploadTextures();
    int addText(TextLabel* text);
    void removeText(TextLabel* text);
    void dispatchMouseButton(int button, int action, int mods,
                             double cursorX, double cursorY);
    bool drawFrame();
    void framebufferResized(int fbWidth, int fbHeight,
                            int winWidth, int winHeight);

    bool isValid() const { return m_context != nullptr; }
    int framebufferWidth() const { return m_fbWidth; }
    int framebufferHeight() const { return m_fbHeight; }

private:
    void updateAnimations(float deltaSeconds);
    void buildTextBatch();

    struct TextDraw {
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        bool blurEnabled = false;
        float blurRadius = 0.0f;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
    };

    void* m_context = nullptr;
    int m_fbWidth = 0, m_fbHeight = 0;
    int m_winWidth = 0, m_winHeight = 0;
    bool m_texturesUploaded = false;
    std::vector<TextureRecord> m_textures;
    std::vector<Rectangle*> m_rectangles;
    std::vector<TextLabel*> m_textLabels;
    FontAtlas m_fontAtlas;
    std::vector<float> m_rectangleVertices;
    std::vector<float> m_rectangleEffects;
    std::vector<float> m_textDrawMetadata;
    std::vector<float> m_textVertices;
    std::vector<uint32_t> m_textIndices;
    std::vector<TextDraw> m_textDraws;
    std::chrono::steady_clock::time_point m_lastFrameTime{};
    static constexpr uint32_t MAX_TEXT_GLYPHS = 4096;
};
