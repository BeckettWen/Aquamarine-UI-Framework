#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceImage;
layout(push_constant) uniform BlurPushConstants {
    vec2 texelSize;
    float sigma;
    float horizontal;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = gl_FragCoord.xy * pc.texelSize;
    if (pc.sigma <= 0.0) {
        vec4 source = texture(sourceImage, uv);
        if (pc.horizontal < -0.5 && source.a > 0.0001)
            source.rgb /= source.a;
        outColor = source;
        return;
    }

    vec2 axis = (pc.horizontal > 0.5)
        ? vec2(pc.texelSize.x, 0.0)
        : vec2(0.0, pc.texelSize.y);
    float sigma = max(pc.sigma, 0.5);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;
    for (int tap = -4; tap <= 4; ++tap) {
        float offset = float(tap) * sigma * 0.5;
        float normalizedOffset = float(tap) * 0.5;
        float weight = exp(-0.5 * normalizedOffset * normalizedOffset);
        sum += texture(sourceImage, uv + axis * offset) * weight;
        weightSum += weight;
    }
    outColor = sum / weightSum;
}
