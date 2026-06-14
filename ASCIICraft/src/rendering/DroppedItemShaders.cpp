#include <ASCIICraft/rendering/DroppedItemShaders.hpp>

namespace DroppedItemShaders {

const char* GetVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float3 cameraPos;
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
    float dist : TEXCOORD1;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    output.dist = distance(input.position, cameraPos);
    return output;
}
)";
}

const char* GetPSSource() {
    return R"(
#include "ColorUtil.hlsl"

Texture2DArray itemTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float3 cameraPos;
    float4 fogParams;
    float3 fogColor;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
    float dist : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = itemTextures.Sample(samplerState, input.texcoord);

    static const float ALPHA_CUTOFF = 0.5;
    clip(texColor.a - ALPHA_CUTOFF);

    float fogFactor = saturate((input.dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 fogLinear = sRGBToLinear(fogColor);
    float3 finalColorLinear = lerp(texColor.rgb, fogLinear, fogFactor);

    return float4(finalColorLinear, texColor.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("cameraPos", ASCIIgL::UniformType::Float3)
        .Add("fogParams", ASCIIgL::UniformType::Float4)
        .Add("fogColor", ASCIIgL::UniformType::Float3)
        .Build();
}

} // namespace DroppedItemShaders
