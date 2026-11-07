#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
#include <string>

// Thin wrapper around a GLFW window created with NO client API (Vulkan only,
// no OpenGL context). Owns the GLFWwindow and tracks window/framebuffer size
// (logical vs framebuffer, for high-DPI) plus cursor input.
//
// The Vulkan surface is created by the Vulkan renderer (it needs the VkInstance
// first), so this class does not own a VkSurfaceKHR.
class Window {
public:
    Window();
    ~Window();

    // Create a resizable GLFW window with GLFW_CLIENT_API = GLFW_NO_API.
    bool create(int width, int height, const char* title);
    void destroy();

    GLFWwindow* handle() const { return m_window; }

    int width() const  { return m_width;  }   // logical window size
    int height() const { return m_height; }
    int fbWidth() const  { return m_fbWidth;  }   // framebuffer size (pixels)
    int fbHeight() const { return m_fbHeight; }

    bool framebufferResized() const { return m_resized; }
    void resetResizeFlag() { m_resized = false; }

    void pollEvents();
    bool shouldClose() const;

    // Cursor position in screen/logical coordinates (top-left origin).
    void cursorPos(double& x, double& y) const;

    using ResizeCallback = std::function<void(int fbWidth, int fbHeight,
                                              int winWidth, int winHeight)>;
    void setResizeCallback(ResizeCallback cb);

private:
    static void onFramebufferResize(GLFWwindow* win, int w, int h);

    GLFWwindow* m_window = nullptr;
    int m_width = 0, m_height = 0;
    int m_fbWidth = 0, m_fbHeight = 0;
    bool m_resized = false;
    ResizeCallback m_resizeCb;
};
