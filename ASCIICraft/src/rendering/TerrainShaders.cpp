#include <ASCIICraft/rendering/TerrainShaders.hpp>

namespace TerrainShaders {

const char* GetTerrainVSSource() {
    // Same as texture array vertex shader - passes position + UVLayer
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;   // Start color of gradient (dark)
    float4 gradientEnd;     // End color of gradient (bright)
    float3 cameraPos;       // Camera world position for fog calculation
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 texcoord : TEXCOORD0;  // UV + Layer index
    float light : LIGHT;           // Per-vertex directional light multiplier
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;  // UV + Layer passed to pixel shader
    float dist : TEXCOORD1;       // Distance from camera
    float light : TEXCOORD2;      // Interpolated light multiplier
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    output.dist = distance(input.position, cameraPos);
    output.light = input.light;
    return output;
}
)";
}

const char* GetTerrainPSSource() {
    return R"(
Texture2DArray blockTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;   // Start color of gradient (dark), alpha = unused
    float4 gradientEnd;     // End color of gradient (bright), alpha = unused
    float3 cameraPos;       // Unused in PS but part of CBuffer layout
    float4 fogParams;       // x=start, y=end
    float3 fogColor;        // Color to fade to at distance (matches background)
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;  // UV.xy + Layer.z
    float dist : TEXCOORD1;       // Distance from camera
    float light : TEXCOORD2;      // Per-vertex directional light
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Sample the texture array
    float4 texColor = blockTextures.Sample(samplerState, input.texcoord);
    
    // Binary alpha test (cutout): discard pixels below threshold (e.g. leaves, grass)
    static const float ALPHA_CUTOFF = 0.5;
    clip(texColor.a - ALPHA_CUTOFF);
    
    // Compute luminance using standard coefficients (Rec. 709)
    float luminance = dot(texColor.rgb, float3(0.2126, 0.7152, 0.0722));
    
    // === Monochromatic gradient mapping (no hue drift) ===
    float3 hue = normalize(gradientEnd.rgb + 0.0001);
    float minBrightness = length(gradientStart.rgb);
    float maxBrightness = length(gradientEnd.rgb);
    float brightness = lerp(minBrightness, maxBrightness, luminance);
    float3 mappedColor = hue * brightness;

    // === Per-vertex directional lighting ===
    mappedColor *= input.light;

    // === Fog Application ===
    float fogFactor = saturate((input.dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 finalColor = lerp(mappedColor, fogColor, fogFactor);
    
    return float4(finalColor, texColor.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetTerrainPSUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Add("cameraPos", ASCIIgL::UniformType::Float3)
        .Add("fogParams", ASCIIgL::UniformType::Float4)
        .Add("fogColor", ASCIIgL::UniformType::Float3)
        .Build();
}

} // namespace TerrainShaders
