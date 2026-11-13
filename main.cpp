// Lyra Phase 3 demo — Vulkan UI framework with textures, text, and animation.
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
#include "Version.hpp"
#include "VulkanRenderer.hpp"
#include "Rectangle.hpp"
#include "Texture.hpp"
#include "Text.hpp"

#include <iostream>
#include <memory>
#include <vector>

#ifndef SHADER_PATH
#define SHADER_PATH "Shaders"
#endif

int main() {
    std::cout << lyra::version::projectName << " " << lyra::version::string
              << " (" << lyra::version::codename << ")\n";
    if (!glfwInit()) {
        std::cerr << "Demo: failed to initialize GLFW\n";
        return 1;
    }

    const int winW = 800;
    const int winH = 600;

    Window window;
    const std::string windowTitle =
        std::string(lyra::version::projectName) + " " + lyra::version::string +
        " (" + lyra::version::codename + ") — Animation and Input";
    if (!window.create(winW, winH, windowTitle.c_str())) {
        std::cerr << "Demo: failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    VulkanRenderer renderer;
    if (!renderer.init(window.handle(),
                       window.fbWidth(), window.fbHeight(),
                       window.width(), window.height(),
                       SHADER_PATH)) {
        std::cerr << "Demo: failed to initialize Vulkan renderer\n";
        renderer.destroy();
        glfwTerminate();
        return 1;
    }

    window.setResizeCallback([&renderer](int fbw, int fbh, int ww, int wh) {
        renderer.framebufferResized(fbw, fbh, ww, wh);
    });
    window.setMouseButtonCallback(
        [&renderer](int button, int action, int mods, double x, double y) {
            renderer.dispatchMouseButton(button, action, mods, x, y);
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

    // Solid blue rectangle (no texture, layer -1), in the upper-left demo zone.
    rects.emplace_back(50.0f, 150.0f, 160.0f, 100.0f,
                       std::array<float, 4>{0.2f, 0.45f, 1.0f, 1.0f});
    rects.back().textureLayer = -1;
    rects.back().onClick = []() {
        std::cout << "click: solid blue rectangle\n";
    };

    // Blue-tinted checkerboard rectangle (layer 0).
    rects.emplace_back(280.0f, 150.0f, 180.0f, 140.0f,
                       std::array<float, 4>{0.45f, 0.7f, 1.0f, 1.0f});
    rects.back().textureLayer = layerRed;
    rects.back().onClick = []() {
        std::cout << "click: checkerboard rectangle\n";
    };

    // Shaded gradient rectangle (layer 1), in its own lower-right zone.
    rects.emplace_back(520.0f, 300.0f, 220.0f, 170.0f,
                       std::array<float, 4>{0.5f, 0.72f, 1.0f, 0.9f});
    rects.back().textureLayer = layerGrad;
    rects.back().onClick = []() {
        std::cout << "click: shaded gradient rectangle\n";
    };

    // Solid blue rectangle (no texture), reserved as the animation zone.
    rects.emplace_back(50.0f, 300.0f, 150.0f, 130.0f,
                       std::array<float, 4>{0.2f, 0.45f, 1.0f, 1.0f});
    rects.back().textureLayer = -1;
    rects.back().onClick = []() {
        std::cout << "click: animated blue rectangle\n";
    };

    // Translucent blue checkerboard glass over the first checkerboard. The
    // backdrop is blurred within this component's bounds; the checker texture
    // itself remains sharp.
    rects.emplace_back(340.0f, 180.0f, 160.0f, 120.0f,
                       std::array<float, 4>{0.2f, 0.45f, 1.0f, 0.78f});
    rects.back().textureLayer = layerRed;
    rects.back().backdropBlurEnabled = true;
    rects.back().backdropBlurRadius = 12.0f;
    rects.back().onClick = []() {
        std::cout << "click: tinted checkerboard rectangle\n";
    };

    // The blue rectangle follows a cubic Bezier path confined to the
    // lower-left zone (repeatCount 0 = forever).
    if (!rects[3].animateBezierTo(50.0f, 430.0f,
                                  80.0f, 300.0f,
                                  80.0f, 430.0f,
                                  3.0f, 0))
        std::cerr << "Demo: failed to start Bezier rectangle animation\n";

    // The renderer stores non-owning pointers; rects must outlive renderer.
    for (auto& r : rects)
        renderer.addRectangle(&r);

    // Text uses an independent glyph texture array, descriptor set, pipeline,
    // and vertex/index batch. Coordinates are top-left-origin; y is the font
    // baseline in logical window pixels.
    std::vector<TextLabel> labels;
    labels.reserve(4);
    labels.push_back({"Cloudsdale", 50.0f, 95.0f, 0.8f,
                      {1.0f, 1.0f, 1.0f, 1.0f}, true});
    labels.push_back({"LYRA", 65.0f, 215.0f, 0.42f,
                      {1.0f, 1.0f, 1.0f, 1.0f}, true});
    labels.push_back({"CHECKER", 295.0f, 225.0f, 0.36f,
                      {1.0f, 1.0f, 1.0f, 1.0f}, true});
    labels.back().backdropBlurEnabled = true;
    labels.back().backdropBlurRadius = 5.0f;
    labels.push_back({"SHADED", 545.0f, 390.0f, 0.5f,
                      {1.0f, 1.0f, 1.0f, 1.0f}, true});
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
