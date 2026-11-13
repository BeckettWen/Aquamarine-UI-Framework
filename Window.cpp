#include "Window.hpp"
#include <iostream>

#if defined(LYRA_ENABLE_MACOS_WINDOW_BLUR)
extern "C" int lyraSetMacOSWindowBackdropBlur(GLFWwindow* window,
                                               int enabled);
#endif

Window::Window() = default;

Window::~Window() { destroy(); }

bool Window::create(int width, int height, const char* title) {
    if (!glfwVulkanSupported()) {
        std::cerr << "Window: GLFW reports no Vulkan support.\n";
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan only, no OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#if defined(LYRA_ENABLE_MACOS_WINDOW_BLUR)
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
#endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Window: failed to create GLFW window.\n";
        return false;
    }

#if defined(LYRA_ENABLE_MACOS_WINDOW_BLUR)
    if (!lyraSetMacOSWindowBackdropBlur(m_window, 1)) {
        std::cerr << "Window: macOS backdrop blur could not be enabled; "
                     "the window will still use its transparent framebuffer\n";
    } else {
        std::cout << "Window: macOS NSVisualEffectView backdrop attached\n";
    }
#endif

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, onFramebufferResize);
    glfwSetMouseButtonCallback(m_window, onMouseButton);

    glfwGetWindowSize(m_window, &m_width, &m_height);
    glfwGetFramebufferSize(m_window, &m_fbWidth, &m_fbHeight);
    return true;
}

void Window::destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Window::pollEvents() { glfwPollEvents(); }

bool Window::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void Window::cursorPos(double& x, double& y) const {
    if (m_window) glfwGetCursorPos(m_window, &x, &y);
    else { x = 0.0; y = 0.0; }
}

void Window::setResizeCallback(ResizeCallback cb) {
    m_resizeCb = std::move(cb);
}

void Window::setMouseButtonCallback(MouseButtonCallback cb) {
    m_mouseButtonCb = std::move(cb);
}

void Window::onFramebufferResize(GLFWwindow* win, int w, int h) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    glfwGetWindowSize(win, &self->m_width, &self->m_height);
    glfwGetFramebufferSize(win, &self->m_fbWidth, &self->m_fbHeight);
    self->m_resized = true;
    if (self->m_resizeCb)
        self->m_resizeCb(self->m_fbWidth, self->m_fbHeight,
                         self->m_width, self->m_height);
}

void Window::onMouseButton(GLFWwindow* win, int button, int action, int mods) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!self || !self->m_mouseButtonCb) return;
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(win, &x, &y);
    self->m_mouseButtonCb(button, action, mods, x, y);
}
