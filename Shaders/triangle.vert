#version 450

// X-19 Vulkan rectangle vertex shader.
//
// Coordinate convention (documented):
//   - Rectangle positions are TOP-LEFT origin (x right, y down), in screen /
//     logical pixels. This matches GLFW's cursor coordinates and Vulkan's
//     framebuffer origin, so rendering and hit-testing agree without any
//     y-axis inversion.
//   - The vertex shader converts screen/logical coords -> framebuffer pixels
//     (using the high-DPI scale) -> Vulkan NDC.
//     Vulkan NDC: x in [-1, 1] (left->right), y in [-1, 1] (bottom->top,
//     y=1 is the top).
//
// Push constants (16 bytes, vertex stage):
//   pc.viewport = framebuffer size in pixels (width, height)
//   pc.scale    = framebuffer / window size ratio (high-DPI scale)

layout(location = 0) in vec2 inPosition;   // screen/logical coords (top-left origin)
layout(location = 1) in vec4 inColor;      // per-vertex RGBA color

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    vec2 viewport;  // framebuffer size (pixels)
    vec2 scale;     // framebuffer / window size ratio
} pc;

void main() {
    vec2 fbPos = inPosition * pc.scale;                 // screen -> framebuffer pixels
    float ndcX = (2.0 * fbPos.x / pc.viewport.x) - 1.0; // left=-1, right=+1
    float ndcY = 1.0 - (2.0 * fbPos.y / pc.viewport.y); // top=+1, bottom=-1
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    fragColor = inColor;
}
