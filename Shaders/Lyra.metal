#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float2 position [[attribute(0)]];
    float4 color [[attribute(1)]];
    float2 uv [[attribute(2)]];
    float layer [[attribute(3)]];
};

struct VertexOutput {
    float4 position [[position]];
    float4 color;
    float2 uv;
    float layer [[flat]];
};

struct ViewportUniforms {
    float2 size;
};

vertex VertexOutput lyraVertex(VertexInput input [[stage_in]],
                               constant ViewportUniforms& viewport [[buffer(1)]]) {
    VertexOutput output;
    float2 ndc = float2(input.position.x * 2.0 / viewport.size.x - 1.0,
                        1.0 - input.position.y * 2.0 / viewport.size.y);
    output.position = float4(ndc, 0.0, 1.0);
    output.color = input.color;
    output.uv = input.uv;
    output.layer = input.layer;
    return output;
}

fragment float4 lyraRectangleFragment(
    VertexOutput input [[stage_in]],
    texture2d_array<float> textureArray [[texture(0)]],
    sampler textureSampler [[sampler(0)]]) {
    float4 texel = input.layer >= 0.0
        ? textureArray.sample(textureSampler, input.uv, uint(input.layer))
        : float4(1.0);
    return texel * input.color;
}

fragment float4 lyraTextFragment(
    VertexOutput input [[stage_in]],
    texture2d_array<float> glyphArray [[texture(0)]],
    sampler glyphSampler [[sampler(0)]]) {
    float coverage = glyphArray.sample(glyphSampler, input.uv,
                                       uint(input.layer)).r;
    return float4(input.color.rgb, input.color.a * coverage);
}

struct FullscreenOutput {
    float4 position [[position]];
    float2 uv;
};

vertex FullscreenOutput lyraFullscreenVertex(uint vertexID [[vertex_id]]) {
    constexpr float2 positions[3] = {
        float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)
    };
    FullscreenOutput output;
    float2 position = positions[vertexID];
    output.position = float4(position, 0.0, 1.0);
    output.uv = float2(position.x * 0.5 + 0.5,
                       0.5 - position.y * 0.5);
    return output;
}

fragment float4 lyraCopyFragment(
    FullscreenOutput input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler sourceSampler [[sampler(0)]]) {
    return source.sample(sourceSampler, input.uv);
}

struct BlurUniforms {
    float2 viewportSize;
    float sigma;
    uint horizontal;
};

fragment float4 lyraBlurFragment(
    FullscreenOutput input [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler sourceSampler [[sampler(0)]],
    constant BlurUniforms& blur [[buffer(0)]]) {
    float2 axis = blur.horizontal != 0
        ? float2(1.0 / blur.viewportSize.x, 0.0)
        : float2(0.0, 1.0 / blur.viewportSize.y);
    float sigma = max(blur.sigma, 0.5);
    float4 total = float4(0.0);
    float weightTotal = 0.0;
    for (int tap = -4; tap <= 4; ++tap) {
        float normalizedOffset = float(tap) * 0.5;
        float weight = exp(-0.5 * normalizedOffset * normalizedOffset);
        total += source.sample(sourceSampler,
                               input.uv + axis * float(tap) * sigma * 0.5) * weight;
        weightTotal += weight;
    }
    return total / weightTotal;
}
