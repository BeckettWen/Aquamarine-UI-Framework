// X-19 Lyra Phase 2 demo — Vulkan-based UI framework with texture engine.
//
// Displays solid-color and textured rectangles as indexed batch draws,
// loads PPM P6 textures from project assets at runtime, and dispatches
// click callbacks. Top-left-origin coordinates; latest-added rectangle is
// topmost for both drawing and click hit-testing.
//
// Build:
//   cmake -S . -B build -DVulkan_ROOT=<path-to-VulkanSDK>/macOS
//   cmake --build build
// Run (from project root, so Shaders/ and Assets/ are found):
//   ./run.sh

#include "Window.hpp"
#include "VulkanRenderer.hpp"
#include "Rectangle.hpp"
#include "Texture.hpp"
#include "Text.hpp"

#include <iostream>
#include <memory>
#include <vector>

int main() {
    if (!glfwInit()) {
        std::cerr << "Demo: failed to initialize GLFW\n";
        return 1;
    }

    const int winW = 800;
    const int winH = 600;

    Window window;
    if (!window.create(winW, winH, "X-19 Lyra Phase 2 — Texture Engine")) {
        std::cerr << "Demo: failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    VulkanRenderer renderer;
    if (!renderer.init(window.handle(),
                       window.fbWidth(), window.fbHeight(),
                       window.width(), window.height(),
                       "Shaders")) {
        std::cerr << "Demo: failed to initialize Vulkan renderer\n";
        renderer.destroy();
        glfwTerminate();
        return 1;
    }

    window.setResizeCallback([&renderer](int fbw, int fbh, int ww, int wh) {
        renderer.framebufferResized(fbw, fbh, ww, wh);
    });

    // --- Load textures from project assets (PPM P6) ---
    // These are genuinely loaded at runtime via the PPM loader, not baked
    // into vertex colors.
    TextureRecord texRed = loadPPM("Assets/checkerboard.ppm");
    TextureRecord texGrad = loadPPM("Assets/gradient.ppm");

    int layerRed = -1;
    int layerGrad = -1;

    if (texRed.valid()) {
        layerRed = renderer.addTexture(std::move(texRed));
        std::cout << "Demo: checkerboard texture -> layer " << layerRed << "\n";
    } else {
        std::cerr << "Demo: failed to load checkerboard.ppm\n";
    }

    if (texGrad.valid()) {
        layerGrad = renderer.addTexture(std::move(texGrad));
        std::cout << "Demo: gradient texture -> layer " << layerGrad << "\n";
    } else {
        std::cerr << "Demo: failed to load gradient.ppm\n";
    }

    // Upload textures to Vulkan before drawing.
    if (!renderer.uploadTextures()) {
        std::cerr << "Demo: failed to upload textures to Vulkan\n";
        renderer.destroy();
        glfwTerminate();
        return 1;
    }

    // --- Rectangles (screen/logical coords, top-left origin) ---
    // Overlap rule: latest-added (topmost) wins clicks.
    std::vector<Rectangle> rects;
    rects.reserve(5);

    // Solid red rectangle (no texture, layer -1)
    rects.emplace_back(50.0f, 50.0f, 200.0f, 150.0f,
                       std::array<float, 4>{1.0f, 0.2f, 0.2f, 1.0f});
    rects.back().textureLayer = -1;
    rects.back().onClick = []() {
        std::cout << "click: solid red rectangle\n";
    };

    // Textured checkerboard rectangle (layer 0)
    rects.emplace_back(300.0f, 50.0f, 200.0f, 150.0f,
                       std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
    rects.back().textureLayer = layerRed;
    rects.back().onClick = []() {
        std::cout << "click: checkerboard rectangle\n";
    };

    // Textured gradient rectangle (layer 1) overlapping checkerboard
    rects.emplace_back(350.0f, 100.0f, 200.0f, 150.0f,
                       std::array<float, 4>{1.0f, 1.0f, 1.0f, 0.9f});
    rects.back().textureLayer = layerGrad;
    rects.back().onClick = []() {
        std::cout << "click: gradient rectangle (topmost in overlap)\n";
    };

    // Solid green rectangle (no texture)
    rects.emplace_back(50.0f, 250.0f, 150.0f, 150.0f,
                       std::array<float, 4>{0.2f, 0.8f, 0.2f, 1.0f});
    rects.back().textureLayer = -1;
    rects.back().onClick = []() {
        std::cout << "click: solid green rectangle\n";
    };

    // Textured checkerboard with tint (layer 0, tinted blue)
    rects.emplace_back(250.0f, 250.0f, 150.0f, 150.0f,
                       std::array<float, 4>{0.3f, 0.5f, 1.0f, 1.0f});
    rects.back().textureLayer = layerRed;
    rects.back().onClick = []() {
        std::cout << "click: tinted checkerboard rectangle\n";
    };

    // The renderer stores non-owning pointers; rects must outlive renderer.
    for (auto& r : rects)
        renderer.addRectangle(&r);

    // Text uses an independent glyph texture array, descriptor set, pipeline,
    // and vertex/index batch. Coordinates are top-left-origin; y is the font
    // baseline in logical window pixels.
    std::vector<TextLabel> labels;
    labels.reserve(2);
    labels.push_back({"LYRA / CLOUDSDALE", 50.0f, 465.0f, 0.72f,
                      {1.0f, 1.0f, 1.0f, 1.0f}, true});
    labels.push_back({"TEXT HAS ITS OWN GLYPH ARRAY", 50.0f, 515.0f, 0.42f,
                      {0.65f, 0.85f, 1.0f, 1.0f}, true});
    for (auto& label : labels)
        renderer.addText(&label);

    while (!window.shouldClose()) {
        window.pollEvents();
        if (!renderer.drawFrame()) break;
    }

    renderer.destroy();
    glfwTerminate();
    return 0;
}
