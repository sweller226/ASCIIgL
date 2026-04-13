#include <ASCIICraft/rendering/MobShaders.hpp>

namespace MobShaders {

const char* GetMobVSSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;
    float4 gradientEnd;
    float3 cameraPos;
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

const char* GetMobPSSource() {
    return R"(
Texture2D mobTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;   // reserved (uniform layout must match VS cbuffer)
    float4 gradientEnd;     // reserved
    float3 cameraPos;
    float4 fogParams;       // reserved
    float3 fogColor;        // reserved
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float dist : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = mobTexture.Sample(samplerState, input.texcoord);

    // Alpha test — discard nearly-transparent pixels so mob silhouettes
    // aren't filled with background-colored quads.
    static const float ALPHA_CUTOFF = 0.5;
    clip(texColor.a - ALPHA_CUTOFF);

    // === Monochromatic gradient mapping (same as terrain) ===
    // Compute luminance using standard coefficients (Rec. 709)
    float luminance = dot(texColor.rgb, float3(0.2126, 0.7152, 0.0722));
    
    // Extract hue direction from the bright end of the gradient
    float3 hue = normalize(gradientEnd.rgb + 0.0001); // Avoid div by zero
    
    // Compute brightness range from gradient endpoints
    float minBrightness = length(gradientStart.rgb);
    float maxBrightness = length(gradientEnd.rgb);
    
    // Map luminance to brightness along the hue direction
    float brightness = lerp(minBrightness, maxBrightness, luminance);
    
    // Reconstruct color = hue direction * brightness
    float3 mappedColor = hue * brightness;
    
    return float4(mappedColor, texColor.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetMobPSUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Add("cameraPos", ASCIIgL::UniformType::Float3)
        .Add("fogParams", ASCIIgL::UniformType::Float4)
        .Add("fogColor", ASCIIgL::UniformType::Float3)
        .Build();
}

} // namespace MobShaders
