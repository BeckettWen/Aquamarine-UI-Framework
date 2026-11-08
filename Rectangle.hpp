#pragma once

#include <array>
#include <functional>

// A rectangle widget with optional texturing and a click callback.
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
// Texturing (Phase 2):
//   - textureLayer: index into the renderer's texture array. Negative means
//     "solid color only" (no texture sample). Zero or more means sample that
//     array layer and multiply by vertex color as a tint.
//   - uv[4]: per-corner UVs in the order TL, TR, BR, BL. (0,0)=top-left,
//     (1,1)=bottom-right in Vulkan framebuffer terms (y down). A full-white
//     default texture lets solid rectangles share the same textured pipeline.
// Ownership: the renderer stores non-owning pointers; the caller must keep
// each Rectangle alive while registered, and remove/destroy rectangles before
// the renderer is destroyed so callbacks never invoke a destroyed object.
class Rectangle {
public:
    float x = 0.0f;          // top-left corner, screen/logical pixels
    float y = 0.0f;          // top-left corner, screen/logical pixels
    float width = 100.0f;
    float height = 100.0f;
    std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f}; // RGBA tint

    int textureLayer = -1;   // -1 = solid color (use default white layer)
    std::array<float, 8> uv = {   // TL, TR, BR, BL (each is u,v)
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
    };

    std::function<void()> onClick; // fires once per click inside this rect
    bool enabled = true;
    bool visible = true;

    Rectangle() = default;
    Rectangle(float x, float y, float w, float h,
              std::array<float, 4> color);

    // Returns true if (cx, cy) — screen/logical coords — is inside this rect.
    bool contains(double cx, double cy) const;

    // Write this rectangle's 4 vertices into vbuf.
    // Layout per vertex: pos(2) + color(4) + uv(2) + layer(1) = 9 floats.
    // vbuf must have room for 4 * 9 floats.
    void writeVertices(float* vbuf) const;

    // Write this rectangle's 6 indices (2 triangles) into ibuf, offset by
    // baseVertex. ibuf must have room for 6 unsigned ints.
    void writeIndices(unsigned int* ibuf, unsigned int baseVertex) const;
};
