#version 450

// Dedicated Lyra glyph pipeline. Text has its own geometry and glyph-array
// resources; it does not share the rectangle texture array.
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in float inGlyphLayer;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) flat out int fragGlyphLayer;

layout(push_constant) uniform TextPushConstants {
    vec2 viewport;
    vec2 scale;
} pc;

void main() {
    vec2 framebufferPosition = inPosition * pc.scale;
    float ndcX = (2.0 * framebufferPosition.x / pc.viewport.x) - 1.0;
    // Match FreeType's top-to-bottom glyph rows and the positive-height
    // Vulkan viewport: screen top y=0 maps to NDC -1 / framebuffer top.
    float ndcY = (2.0 * framebufferPosition.y / pc.viewport.y) - 1.0;
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    fragColor = inColor;
    fragUV = inUV;
    fragGlyphLayer = int(inGlyphLayer + 0.5);
}
