#version 450

// X-19 Vulkan rectangle fragment shader. Passes the interpolated vertex color
// straight through to the framebuffer (with alpha blending configured in the
// pipeline).

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
