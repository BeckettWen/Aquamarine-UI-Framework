#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "MetalRenderer.hpp"
#include "Rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

#ifndef FONT_PATH
#define FONT_PATH "/System/Library/Fonts/SFNS.ttf"
#endif
#ifndef LYRA_METALLIB_PATH
#define LYRA_METALLIB_PATH "LyraShaders.metallib"
#endif

extern "C" void* lyraMetalCreateForGLFW(GLFWwindow*, int32_t, int32_t,
                                         int32_t, int32_t,
                                         const char*);
extern "C" void lyraMetalDestroy(void*);
extern "C" void lyraMetalResize(void*, int32_t, int32_t);
extern "C" int32_t lyraMetalUploadTextures(void*, const uint8_t*, int32_t,
                                            int32_t, int32_t);
extern "C" int32_t lyraMetalUploadGlyphAtlas(void*, const uint8_t*, int32_t,
                                             int32_t, int32_t);
extern "C" int32_t lyraMetalRender(void*, const float*, const float*, int32_t,
                                    const float*, int32_t, const uint32_t*,
                                    int32_t, const float*, int32_t,
                                    int32_t, int32_t, int32_t, int32_t);
extern "C" void* lyraSwiftMetalCreate(void*, int32_t, int32_t, int32_t,
                                      int32_t, const char*);

extern "C" void* lyraMetalCreateForGLFW(GLFWwindow* window,
                                         int32_t width, int32_t height,
                                         int32_t windowBlurEnabled,
                                         int32_t inAppBlurEnabled,
                                         const char* metallibPath) {
    if (!window) return nullptr;
    return lyraSwiftMetalCreate((__bridge void*)glfwGetCocoaWindow(window),
                                width, height, windowBlurEnabled,
                                inAppBlurEnabled, metallibPath);
}

MetalRenderer::~MetalRenderer() { destroy(); }

bool MetalRenderer::init(GLFWwindow* window, int fbWidth, int fbHeight,
                         int winWidth, int winHeight,
                         const char* unusedShaderDir) {
    (void)unusedShaderDir;
    destroy();
    if (!window) return false;

    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_winWidth = winWidth;
    m_winHeight = winHeight;

    m_context = lyraMetalCreateForGLFW(
        window, fbWidth, fbHeight,
#if defined(LYRA_ENABLE_MACOS_WINDOW_BLUR)
        1,
#else
        0,
#endif
#if defined(LYRA_ENABLE_BACKDROP_BLUR)
        1,
#else
        0,
#endif
        LYRA_METALLIB_PATH);
    if (!m_context) {
        std::cerr << "Metal renderer: failed to create the Swift/Metal backend\n";
        return false;
    }

    if (!m_fontAtlas.load(FONT_PATH, 48) ||
        lyraMetalUploadGlyphAtlas(m_context, m_fontAtlas.pixels().data(),
                                  m_fontAtlas.width(), m_fontAtlas.height(),
                                  static_cast<int32_t>(m_fontAtlas.layerCount())) == 0) {
        std::cerr << "Metal renderer: failed to upload the FreeType glyph atlas\n";
        destroy();
        return false;
    }
    m_lastFrameTime = std::chrono::steady_clock::now();
    std::cout << "Metal renderer: initialized (" << m_fbWidth << "x"
              << m_fbHeight << " framebuffer)\n";
    return true;
}

void MetalRenderer::destroy() {
    if (m_context) lyraMetalDestroy(m_context);
    m_context = nullptr;
    m_textures.clear();
    m_texturesUploaded = false;
    m_lastFrameTime = {};
}

int MetalRenderer::addRectangle(Rectangle* rectangle) {
    if (!rectangle) return -1;
    m_rectangles.push_back(rectangle);
    return static_cast<int>(m_rectangles.size()) - 1;
}

void MetalRenderer::removeRectangle(Rectangle* rectangle) {
    m_rectangles.erase(std::remove(m_rectangles.begin(), m_rectangles.end(), rectangle),
                       m_rectangles.end());
}

int MetalRenderer::addTexture(TextureRecord texture) {
    if (!texture.valid()) {
        std::cerr << "Metal renderer: cannot register an invalid texture\n";
        return -1;
    }
    if (texture.channels != 3 && texture.channels != 4) {
        std::cerr << "Metal renderer: textures must use RGB or RGBA pixels\n";
        return -1;
    }
    if (texture.width <= 0 || texture.height <= 0) {
        std::cerr << "Metal renderer: texture pixel buffer has invalid dimensions\n";
        return -1;
    }
    const size_t width = static_cast<size_t>(texture.width);
    const size_t height = static_cast<size_t>(texture.height);
    const size_t channels = static_cast<size_t>(texture.channels);
    if (width > std::numeric_limits<size_t>::max() / height ||
        width * height > std::numeric_limits<size_t>::max() / channels ||
        texture.pixels.size() < width * height * channels) {
        std::cerr << "Metal renderer: texture pixel buffer has invalid dimensions\n";
        return -1;
    }
    if (!m_textures.empty() &&
        (texture.width != m_textures.front().width ||
         texture.height != m_textures.front().height)) {
        std::cerr << "Metal renderer: texture-array images must have equal dimensions\n";
        return -1;
    }
    m_textures.push_back(std::move(texture));
    m_texturesUploaded = false;
    return static_cast<int>(m_textures.size()) - 1;
}

bool MetalRenderer::uploadTextures() {
    if (!m_context) return false;
    if (m_textures.empty()) {
        m_texturesUploaded = true;
        return true;
    }

    const int width = m_textures.front().width;
    const int height = m_textures.front().height;
    if (height <= 0 || width <= 0) {
        std::cerr << "Metal renderer: texture array size overflow\n";
        return false;
    }
    const size_t pixelsPerLayer = static_cast<size_t>(width) *
                                  static_cast<size_t>(height);
    if (pixelsPerLayer > std::numeric_limits<size_t>::max() / 4) {
        std::cerr << "Metal renderer: texture array size overflow\n";
        return false;
    }
    const size_t layerBytes = pixelsPerLayer * 4;
    if (m_textures.size() > std::numeric_limits<size_t>::max() / layerBytes) {
        std::cerr << "Metal renderer: texture array size overflow\n";
        return false;
    }
    std::vector<uint8_t> rgba(layerBytes * m_textures.size());
    for (size_t layer = 0; layer < m_textures.size(); ++layer) {
        const TextureRecord& texture = m_textures[layer];
        uint8_t* destination = rgba.data() + layer * layerBytes;
        for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height; ++pixel) {
            const size_t source = pixel * static_cast<size_t>(texture.channels);
            const size_t target = pixel * 4;
            destination[target + 0] = texture.pixels[source + 0];
            destination[target + 1] = texture.pixels[source + 1];
            destination[target + 2] = texture.pixels[source + 2];
            destination[target + 3] = texture.channels == 4
                ? texture.pixels[source + 3] : 255;
        }
    }
    const bool uploaded = lyraMetalUploadTextures(
        m_context, rgba.data(), width, height,
        static_cast<int32_t>(m_textures.size())) != 0;
    m_texturesUploaded = uploaded;
    if (!uploaded) std::cerr << "Metal renderer: texture upload failed\n";
    return uploaded;
}

int MetalRenderer::addText(TextLabel* text) {
    if (!text) return -1;
    m_textLabels.push_back(text);
    return static_cast<int>(m_textLabels.size()) - 1;
}

void MetalRenderer::removeText(TextLabel* text) {
    m_textLabels.erase(std::remove(m_textLabels.begin(), m_textLabels.end(), text),
                       m_textLabels.end());
}

void MetalRenderer::dispatchMouseButton(int button, int action, int mods,
                                        double cursorX, double cursorY) {
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    for (auto it = m_rectangles.rbegin(); it != m_rectangles.rend(); ++it) {
        Rectangle* rectangle = *it;
        if (rectangle && rectangle->enabled && rectangle->visible &&
            rectangle->contains(cursorX, cursorY)) {
            auto callback = rectangle->onClick;
            if (callback) callback();
            return;
        }
    }
}

void MetalRenderer::updateAnimations(float deltaSeconds) {
    for (Rectangle* rectangle : m_rectangles)
        if (rectangle) rectangle->advanceAnimation(deltaSeconds);
}

void MetalRenderer::buildTextBatch() {
    m_textVertices.clear();
    m_textIndices.clear();
    m_textDraws.clear();
    if (!m_fontAtlas.valid()) return;

    uint32_t glyphCount = 0;
    const float atlasWidth = static_cast<float>(m_fontAtlas.width());
    const float atlasHeight = static_cast<float>(m_fontAtlas.height());
    bool capacityWarningPrinted = false;

    for (const TextLabel* label : m_textLabels) {
        if (!label || !label->visible || label->text.empty() ||
            !std::isfinite(label->scale) || label->scale <= 0.0f)
            continue;

        TextDraw draw;
        draw.firstIndex = static_cast<uint32_t>(m_textIndices.size());
        draw.blurEnabled = label->backdropBlurEnabled;
        draw.blurRadius = label->backdropBlurRadius;
        float cursorX = label->x;
        float baselineY = label->baselineY;

        for (unsigned char character : label->text) {
            if (character == '\n') {
                cursorX = label->x;
                baselineY += static_cast<float>(m_fontAtlas.lineHeight()) * label->scale;
                continue;
            }
            if (character == '\r') continue;
            const FontGlyph* glyph = m_fontAtlas.glyph(character);
            if (!glyph) continue;
            if (character == '\t') {
                const FontGlyph* space = m_fontAtlas.glyph(' ');
                cursorX += (space ? space->advance : 12.0f) * 4.0f * label->scale;
                continue;
            }

            if (glyph->width > 0 && glyph->height > 0) {
                if (glyphCount >= MAX_TEXT_GLYPHS) {
                    if (!capacityWarningPrinted) {
                        std::cerr << "Metal renderer: text batch limit reached; "
                                     "remaining glyphs were truncated\n";
                        capacityWarningPrinted = true;
                    }
                    break;
                }
                const float left = cursorX + glyph->bearingX * label->scale;
                const float top = baselineY - glyph->bearingY * label->scale;
                const float width = glyph->width * label->scale;
                const float height = glyph->height * label->scale;
                const float right = left + width;
                const float bottom = top + height;
                const float uvRight = static_cast<float>(glyph->width) / atlasWidth;
                const float uvBottom = static_cast<float>(glyph->height) / atlasHeight;
                const float vertices[4][9] = {
                    {left, top, label->color[0], label->color[1], label->color[2], label->color[3], 0, 0, static_cast<float>(glyph->layer)},
                    {right, top, label->color[0], label->color[1], label->color[2], label->color[3], uvRight, 0, static_cast<float>(glyph->layer)},
                    {right, bottom, label->color[0], label->color[1], label->color[2], label->color[3], uvRight, uvBottom, static_cast<float>(glyph->layer)},
                    {left, bottom, label->color[0], label->color[1], label->color[2], label->color[3], 0, uvBottom, static_cast<float>(glyph->layer)},
                };
                const uint32_t baseVertex = glyphCount * 4;
                for (const auto& vertex : vertices)
                    m_textVertices.insert(m_textVertices.end(), std::begin(vertex),
                                          std::end(vertex));
                const uint32_t indices[6] = {baseVertex, baseVertex + 1,
                    baseVertex + 2, baseVertex, baseVertex + 2, baseVertex + 3};
                m_textIndices.insert(m_textIndices.end(), std::begin(indices),
                                     std::end(indices));

                if (draw.indexCount == 0) {
                    draw.x = left;
                    draw.y = top;
                    draw.width = width;
                    draw.height = height;
                } else {
                    const float oldRight = draw.x + draw.width;
                    const float oldBottom = draw.y + draw.height;
                    draw.x = std::min(draw.x, left);
                    draw.y = std::min(draw.y, top);
                    draw.width = std::max(oldRight, right) - draw.x;
                    draw.height = std::max(oldBottom, bottom) - draw.y;
                }
                draw.indexCount += 6;
                ++glyphCount;
            }
            cursorX += glyph->advance * label->scale;
        }
        if (draw.indexCount > 0) m_textDraws.push_back(draw);
        if (glyphCount >= MAX_TEXT_GLYPHS) break;
    }
}

bool MetalRenderer::drawFrame() {
    if (!m_context) return false;
    if (!m_texturesUploaded) {
        std::cerr << "Metal renderer: call uploadTextures() after registering textures\n";
        return false;
    }
    if (m_fbWidth <= 0 || m_fbHeight <= 0) return true;
    const auto now = std::chrono::steady_clock::now();
    float deltaSeconds = 0.0f;
    if (m_lastFrameTime.time_since_epoch().count() != 0)
        deltaSeconds = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
    updateAnimations(std::clamp(deltaSeconds, 0.0f, 0.1f));
    buildTextBatch();

    m_rectangleVertices.clear();
    m_rectangleEffects.clear();
    if (m_rectangleVertices.capacity() < m_rectangles.size() * 36)
        m_rectangleVertices.reserve(m_rectangles.size() * 36);
    if (m_rectangleEffects.capacity() < m_rectangles.size() * 6)
        m_rectangleEffects.reserve(m_rectangles.size() * 6);
    for (const Rectangle* rectangle : m_rectangles) {
        if (!rectangle || !rectangle->visible) continue;
        float vertices[36];
        rectangle->writeVertices(vertices);
        m_rectangleVertices.insert(m_rectangleVertices.end(), std::begin(vertices),
                                   std::end(vertices));
        m_rectangleEffects.insert(m_rectangleEffects.end(), {
#if defined(LYRA_ENABLE_BACKDROP_BLUR)
            rectangle->backdropBlurEnabled ? 1.0f : 0.0f,
#else
            0.0f,
#endif
            rectangle->backdropBlurRadius, rectangle->x, rectangle->y,
            rectangle->width, rectangle->height});
    }

    m_textDrawMetadata.clear();
    if (m_textDrawMetadata.capacity() < m_textDraws.size() * 8)
        m_textDrawMetadata.reserve(m_textDraws.size() * 8);
    for (const TextDraw& draw : m_textDraws) {
        m_textDrawMetadata.insert(m_textDrawMetadata.end(), {
            static_cast<float>(draw.firstIndex),
            static_cast<float>(draw.indexCount),
#if defined(LYRA_ENABLE_BACKDROP_BLUR)
            draw.blurEnabled ? 1.0f : 0.0f,
#else
            0.0f,
#endif
            draw.blurRadius, draw.x, draw.y, draw.width, draw.height});
    }

    return lyraMetalRender(
        m_context, m_rectangleVertices.empty() ? nullptr : m_rectangleVertices.data(),
        m_rectangleEffects.empty() ? nullptr : m_rectangleEffects.data(),
        static_cast<int32_t>(m_rectangleEffects.size() / 6),
        m_textVertices.empty() ? nullptr : m_textVertices.data(),
        static_cast<int32_t>(m_textVertices.size() / 9),
        m_textIndices.empty() ? nullptr : m_textIndices.data(),
        static_cast<int32_t>(m_textIndices.size()),
        m_textDrawMetadata.empty() ? nullptr : m_textDrawMetadata.data(),
        static_cast<int32_t>(m_textDraws.size()),
        m_fbWidth, m_fbHeight, m_winWidth, m_winHeight) != 0;
}

void MetalRenderer::framebufferResized(int fbWidth, int fbHeight,
                                       int winWidth, int winHeight) {
    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_winWidth = winWidth;
    m_winHeight = winHeight;
    if (m_context) lyraMetalResize(m_context, fbWidth, fbHeight);
}
