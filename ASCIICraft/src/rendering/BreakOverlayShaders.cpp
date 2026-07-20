#include <ASCIICraft/rendering/BreakOverlayShaders.hpp>

namespace BreakOverlayShaders {

const char* GetVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 texcoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    return output;
}
)";
}

const char* GetPSSource() {
    return R"(
Texture2DArray blockTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = blockTextures.Sample(samplerState, input.texcoord);

    // Crack decal: match the terrain cutout so crack pixels either show or discard.
    static const float ALPHA_CUTOFF = 0.5;
    clip(texColor.a - ALPHA_CUTOFF);

    return texColor;
}
)";
}

ASCIIgL::UniformBufferLayout GetUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Build();
}

} // namespace BreakOverlayShaders
