#version 450

// X-19 Lyra Phase 2 textured vertex shader.
//
// Per-vertex layout (9 floats, stride 36 bytes):
//   loc 0: pos  vec2  (screen/logical pixels, top-left origin)
//   loc 1: color vec4 (RGBA tint)
//   loc 2: uv    vec2  (0,0 = top-left, 1,1 = bottom-right)
//
// Push constants (20 bytes, vertex + fragment):
//   pc.viewport = framebuffer size in pixels (width, height)
//   pc.scale    = framebuffer / window size ratio (high-DPI scale)
//   pc.layer    = texture array layer (-1 = solid color, >=0 = sample layer)
//
// Screen -> framebuffer -> Vulkan NDC conversion (top-left origin preserved).

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

layout(push_constant) uniform PushConstants {
    vec2 viewport;  // framebuffer size (pixels)
    vec2 scale;     // framebuffer / window size ratio
    int layer;      // texture array layer (-1 = solid)
} pc;

void main() {
    vec2 fbPos = inPosition * pc.scale;
    float ndcX = (2.0 * fbPos.x / pc.viewport.x) - 1.0;
    // With Vulkan's positive-height viewport, NDC y=-1 maps to the top of
    // the framebuffer. Keep the screen's top-left origin by mapping top y=0
    // to -1 (not +1).
    float ndcY = (2.0 * fbPos.y / pc.viewport.y) - 1.0;
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    fragColor = inColor;
    fragUV = inUV;
}
