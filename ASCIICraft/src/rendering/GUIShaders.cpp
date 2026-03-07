#include <ASCIICraft/rendering/GUIShaders.hpp>

namespace GUIShaders {

const char* GetGUIVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;
    float4 gradientEnd;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
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

const char* GetGUIPSSource() {
    return R"(
Texture2D guiTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = guiTexture.Sample(samplerState, input.texcoord);
    return texColor;
}
)";
}

ASCIIgL::UniformBufferLayout GetGUIPSUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Build();
}

const char* GetItemVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;
    float4 gradientEnd;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 texcoord : TEXCOORD0; // UV + Layer index
    float light : LIGHT;          // Per-vertex light (1.0 = full bright for GUI)
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
    float light : TEXCOORD1;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    output.light = input.light;
    return output;
}
)";
}

const char* GetItemPSSource() {
    return R"(
Texture2DArray itemTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
    float light : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = itemTextures.Sample(samplerState, input.texcoord);
    if (texColor.a < 0.1) discard;
    float3 mappedColor = texColor.rgb * input.light;
    return float4(mappedColor, texColor.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetItemPSUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Build();
}

} // namespace GUIShaders