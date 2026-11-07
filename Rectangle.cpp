#include "Rectangle.hpp"

Rectangle::Rectangle(float x, float y, float w, float h,
                     std::array<float, 4> color)
    : x(x), y(y), width(w), height(h), color(color) {}

bool Rectangle::contains(double cx, double cy) const {
    if (!enabled || !visible) return false;
    return cx >= x && cx <= x + width && cy >= y && cy <= y + height;
}

void Rectangle::writeVertices(float* vbuf) const {
    // Rectangle corners, top-left origin (screen coords):
    //   v0 top-left, v1 top-right, v2 bottom-right, v3 bottom-left.
    // Each vertex: (x, y, r, g, b, a).
    float verts[4][6] = {
        {x,         y,         color[0], color[1], color[2], color[3]}, // TL
        {x + width, y,         color[0], color[1], color[2], color[3]}, // TR
        {x + width, y + height,color[0], color[1], color[2], color[3]}, // BR
        {x,         y + height,color[0], color[1], color[2], color[3]}, // BL
    };
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 6; ++j)
            vbuf[i * 6 + j] = verts[i][j];
}

void Rectangle::writeIndices(unsigned int* ibuf, unsigned int baseVertex) const {
    // Two triangles: (0,1,2) and (0,2,3). In screen coords this is clockwise;
    // the vertex shader's y-flip makes it CCW in Vulkan NDC, matching a
    // back-face-cull-none pipeline (we use cull none, so winding is cosmetic).
    ibuf[0] = baseVertex + 0; ibuf[1] = baseVertex + 1; ibuf[2] = baseVertex + 2;
    ibuf[3] = baseVertex + 0; ibuf[4] = baseVertex + 2; ibuf[5] = baseVertex + 3;
}
