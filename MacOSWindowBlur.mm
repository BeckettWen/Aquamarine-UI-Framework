#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <cstdint>

extern "C" int lyraSwiftSetWindowBackdropBlur(void* nativeWindow,
                                                int32_t enabled);

extern "C" int lyraSetMacOSWindowBackdropBlur(GLFWwindow* window,
                                               int enabled) {
    if (!window) return 0;
    return lyraSwiftSetWindowBackdropBlur(
        reinterpret_cast<void*>(glfwGetCocoaWindow(window)), enabled);
}
