#pragma once

#include <array>
#include <functional>

// A rectangle widget with a click callback.
//
// Coordinate convention (documented, used consistently by renderer + input):
//   TOP-LEFT origin: (x, y) is the top-left corner, y increases downward,
//   in screen / logical pixels. This matches GLFW's cursor coordinates and
//   Vulkan's framebuffer origin, so rendering and hit-testing agree without
//   any y-axis inversion.
//
// Overlap rule (documented): rectangles are stored in draw order. The
// latest-added rectangle is topmost (drawn last, checked first for clicks).
// The FIRST rectangle (scanning topmost->backmost) that contains the cursor
// wins the click; its callback fires exactly once.
//
// Ownership: the renderer stores non-owning pointers; the caller must keep
// each Rectangle alive (and remove it from the renderer) while it is in use,
// and must remove/destroy rectangles before the renderer is destroyed, so
// callbacks never invoke a destroyed object.
class Rectangle {
public:
    float x = 0.0f;          // top-left corner, screen/logical pixels
    float y = 0.0f;          // top-left corner, screen/logical pixels
    float width = 100.0f;
    float height = 100.0f;
    std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f}; // RGBA

    std::function<void()> onClick; // fires once per click inside this rect
    bool enabled = true;
    bool visible = true;

    Rectangle() = default;
    Rectangle(float x, float y, float w, float h,
              std::array<float, 4> color);

    // Returns true if (cx, cy) — screen/logical coords — is inside this rect.
    bool contains(double cx, double cy) const;

    // Write this rectangle's 4 vertices (pos vec2 + color vec4) into vbuf.
    // vbuf must have room for 4 * 6 floats.
    void writeVertices(float* vbuf) const;

    // Write this rectangle's 6 indices (2 triangles) into ibuf, offset by
    // baseVertex. ibuf must have room for 6 unsigned ints.
    void writeIndices(unsigned int* ibuf, unsigned int baseVertex) const;
};
