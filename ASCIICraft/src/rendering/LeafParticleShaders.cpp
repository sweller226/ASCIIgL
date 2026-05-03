// ASCIICraft/rendering/LeafParticleShaders.cpp
#include <ASCIICraft/rendering/LeafParticleShaders.hpp>

namespace LeafParticleShaders {

const char* GetVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float    opacityLevel;  // 0.0 = fully faded, 1.0 = fully opaque
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
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.color    = input.color;
    return output;
}
)";
}

const char* GetPSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float    opacityLevel;  // 0.0 = fully faded, 1.0 = fully opaque
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = input.color;
    color.a *= opacityLevel;

    // Discard nearly invisible particles to avoid overdraw cost
    clip(color.a - 0.01f);

    return color;
}
)";
}

ASCIIgL::UniformBufferLayout GetUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp",          ASCIIgL::UniformType::Mat4)
        .Add("opacityLevel", ASCIIgL::UniformType::Float)
        .Build();
}

} // namespace LeafParticleShaders