#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) flat in int fragGlyphLayer;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2DArray glyphArray;

void main() {
    float coverage = texture(glyphArray,
                             vec3(fragUV, float(fragGlyphLayer))).r;
    outColor = vec4(fragColor.rgb, fragColor.a * coverage);
}
