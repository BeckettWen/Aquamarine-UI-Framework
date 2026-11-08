#include "Rectangle.hpp"

Rectangle::Rectangle(float x, float y, float w, float h,
                     std::array<float, 4> color)
    : x(x), y(y), width(w), height(h), color(color) {}

bool Rectangle::contains(double cx, double cy) const {
    if (!enabled || !visible) return false;
    return cx >= x && cx <= x + width && cy >= y && cy <= y + height;
}

void Rectangle::writeVertices(float* vbuf) const {
    // Per-vertex layout (9 floats): pos(2) color(4) uv(2) layer(1)
    // Corners, top-left origin: v0 TL, v1 TR, v2 BR, v3 BL.
    struct Corner { float x, y; float u, v; };
    Corner corners[4] = {
        {x,         y,         uv[0], uv[1]},                       // TL
        {x + width, y,         uv[2], uv[3]},                       // TR
        {x + width, y + height,uv[4], uv[5]},                       // BR
        {x,         y + height,uv[6], uv[7]},                       // BL
    };
    float layer = static_cast<float>(textureLayer);
    for (int i = 0; i < 4; ++i) {
        float* v = &vbuf[i * 9];
        v[0] = corners[i].x;   v[1] = corners[i].y;                 // pos
        v[2] = color[0];       v[3] = color[1];                     // color
        v[4] = color[2];       v[5] = color[3];
        v[6] = corners[i].u;   v[7] = corners[i].v;                 // uv
        v[8] = layer;                                      // layer
    }
}

void Rectangle::writeIndices(unsigned int* ibuf, unsigned int baseVertex) const {
    // Two triangles: (0,1,2) and (0,2,3).
    ibuf[0] = baseVertex + 0; ibuf[1] = baseVertex + 1; ibuf[2] = baseVertex + 2;
    ibuf[3] = baseVertex + 0; ibuf[4] = baseVertex + 2; ibuf[5] = baseVertex + 3;
}
