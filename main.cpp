// X-19 (Lyra) demo — Vulkan-based UI framework.
//
// Draws a few colored rectangles as a single indexed batch and dispatches
// click callbacks. Top-left-origin coordinates; latest-added rectangle is
// topmost for both drawing and click hit-testing.
//
// Build:
//   cmake -S . -B build -DVulkan_ROOT=<path-to-VulkanSDK>/macOS
//   cmake --build build
// Run (from project root, so Shaders/ is found):
//   VULKAN_SDK=<path-to-VulkanSDK> \
//   DYLD_LIBRARY_PATH=<path-to-VulkanSDK>/macOS/lib \
//   VK_ICD_FILENAMES=<path-to-VulkanSDK>/macOS/share/vulkan/icd.d/MoltenVK_icd.json \
//   ./build/x19

#include "Window.hpp"
#include "VulkanRenderer.hpp"
#include "Rectangle.hpp"

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
    if (!window.create(winW, winH, "X-19 Vulkan — rectangle + click")) {
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
        glfwTerminate();
        return 1;
    }

    // Resize: mark the swapchain outdated so the next frame rebuilds it.
    window.setResizeCallback([&renderer](int fbw, int fbh, int ww, int wh) {
        renderer.framebufferResized(fbw, fbh, ww, wh);
    });

    // Rectangles (screen/logical coords, top-left origin).
    // Overlap rule: latest-added (topmost) wins clicks.
    std::vector<Rectangle> rects;
    rects.reserve(3);

    rects.emplace_back(100.0f, 100.0f, 200.0f, 150.0f,
                       std::array<float, 4>{1.0f, 0.2f, 0.2f, 1.0f});
    rects.back().onClick = []() {
        std::cout << "click: red rectangle (rect 1)\n";
    };

    rects.emplace_back(350.0f, 220.0f, 200.0f, 120.0f,
                       std::array<float, 4>{0.2f, 0.8f, 0.2f, 1.0f});
    rects.back().onClick = []() {
        std::cout << "click: green rectangle (rect 2)\n";
    };

    // Overlaps rect 1 -> topmost; clicks in the overlap hit rect 3 first.
    rects.emplace_back(150.0f, 150.0f, 150.0f, 150.0f,
                       std::array<float, 4>{0.2f, 0.4f, 1.0f, 0.9f});
    rects.back().onClick = []() {
        std::cout << "click: blue rectangle (rect 3, topmost)\n";
    };

    // The renderer stores non-owning pointers; rects must outlive the renderer.
    for (auto& r : rects)
        renderer.addRectangle(&r);

    while (!window.shouldClose()) {
        window.pollEvents();
        if (!renderer.drawFrame()) break;
    }

    renderer.destroy();
    glfwTerminate();
    return 0;
}
