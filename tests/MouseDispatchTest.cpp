#include "Rectangle.hpp"
#include "VulkanRenderer.hpp"

#include <cassert>
#include <iostream>

int main() {
    VulkanRenderer renderer;
    Rectangle bottom(0.0f, 0.0f, 100.0f, 100.0f,
                     {1.0f, 0.0f, 0.0f, 1.0f});
    Rectangle top(25.0f, 25.0f, 50.0f, 50.0f,
                  {0.0f, 1.0f, 0.0f, 1.0f});
    int bottomClicks = 0;
    int topClicks = 0;
    bottom.onClick = [&] { ++bottomClicks; };
    top.onClick = [&] { ++topClicks; };
    renderer.addRectangle(&bottom);
    renderer.addRectangle(&top);

    renderer.dispatchMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0, 40.0, 40.0);
    assert(topClicks == 1 && bottomClicks == 0); // topmost wins

    top.enabled = false;
    renderer.dispatchMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0, 40.0, 40.0);
    assert(topClicks == 1 && bottomClicks == 1); // disabled top lets lower widget receive

    renderer.dispatchMouseButton(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0, 40.0, 40.0);
    renderer.dispatchMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0, 40.0, 40.0);
    renderer.dispatchMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0, 140.0, 140.0);
    assert(topClicks == 1 && bottomClicks == 1);

    std::cout << "Mouse dispatch binding and hit testing passed\n";
    return 0;
}
