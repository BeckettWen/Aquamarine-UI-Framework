#include "Window.hpp"
#include <iostream>

Window::Window() = default;

Window::~Window() { destroy(); }

bool Window::create(int width, int height, const char* title) {
    if (!glfwVulkanSupported()) {
        std::cerr << "Window: GLFW reports no Vulkan support.\n";
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan only, no OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Window: failed to create GLFW window.\n";
        return false;
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, onFramebufferResize);

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
