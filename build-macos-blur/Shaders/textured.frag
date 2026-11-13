#version 450

// X-19 Lyra Phase 2 textured fragment shader.
//
// Inputs:
//   fragColor : per-vertex RGBA color (tint)
//   fragUV    : per-vertex UV (0,0 top-left, 1,1 bottom-right)
// Push constants:
//   pc.layer : -1 = solid color (output fragColor only)
//              >= 0 = sample texture array layer pc.layer, multiply by fragColor
//
// Binding 0: combined image sampler (VK_IMAGE_VIEW_TYPE_2D_ARRAY)

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec2 viewport;
    vec2 scale;
    int layer;
} pc;

layout(binding = 0) uniform sampler2DArray texArray;

void main() {
    if (pc.layer >= 0) {
        // Sample the texture array layer. UV y is already top-down
        // (matching framebuffer origin), so no vertical flip is needed.
        vec4 tex = texture(texArray, vec3(fragUV, float(pc.layer)));
        outColor = tex * fragColor;
    } else {
        // Solid color path: no texture sample, output vertex color directly.
        outColor = fragColor;
    }
}
