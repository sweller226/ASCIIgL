#include <ASCIICraft/rendering/BlockTargetOutlineShaders.hpp>

namespace BlockTargetOutlineShaders {

const char* GetVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
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
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    clip(input.color.a - 0.01f);
    return input.color;
}
)";
}

ASCIIgL::UniformBufferLayout GetUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Build();
}

} // namespace BlockTargetOutlineShaders
