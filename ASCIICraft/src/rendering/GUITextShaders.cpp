#include <ASCIICraft/rendering/GUITextShaders.hpp>

namespace GUITextShaders {

const char* GetGUITextVSSource() {
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
    float3 texcoord : TEXCOORD0; // UV + layer index
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

const char* GetGUITextPSSource() {
    return R"(
Texture2DArray fontTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;
    float4 gradientEnd;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = fontTextures.Sample(samplerState, input.texcoord);
    if (texColor.a < 0.1) discard;

    // Grayscale font atlas: use luminance to map into palette gradient.
    float luminance = dot(texColor.rgb, float3(0.299, 0.587, 0.114));
    float3 mapped = lerp(gradientStart.rgb, gradientEnd.rgb, saturate(luminance));
    return float4(mapped, texColor.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetGUITextPSUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Build();
}

} // namespace GUITextShaders