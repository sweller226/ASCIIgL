#include <ASCIICraft/rendering/LeafParticleShaders.hpp>

namespace LeafParticleShaders {

const char* GetVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float3 worldPos;
    float3 cameraPos;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float dist : TEXCOORD1;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.color    = input.color;
    output.dist = distance(worldPos, cameraPos);
    return output;
}
)";
}

const char* GetPSSource() {
    return R"(
#include "ColorUtil.hlsl"

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float3 worldPos;
    float3 cameraPos;
    float4 fogParams;
    float3 fogColor;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float dist : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    clip(input.color.a - 0.01f);

    float fogFactor = saturate((input.dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 fogLinear = sRGBToLinear(fogColor);
    float3 finalColor = lerp(input.color.rgb, fogLinear, fogFactor);

    return float4(finalColor, input.color.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("worldPos", ASCIIgL::UniformType::Float3)
        .Add("cameraPos", ASCIIgL::UniformType::Float3)
        .Add("fogParams", ASCIIgL::UniformType::Float4)
        .Add("fogColor", ASCIIgL::UniformType::Float3)
        .Build();
}

} // namespace LeafParticleShaders
