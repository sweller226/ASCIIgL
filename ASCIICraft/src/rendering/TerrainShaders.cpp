#include <ASCIICraft/rendering/TerrainShaders.hpp>

#include <string>

#include <ASCIICraft/textures/BlockTextureCatalog.hpp>

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
    float3 texcoord : TEXCOORD0;  // UV.xy + Layer.z
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
    float dist : TEXCOORD1;
    nointerpolation float waterPhaseOffset : TEXCOORD2;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    output.dist = distance(input.position, cameraPos);

    float2 tileCoord = floor(input.position.xz);
    float rand = frac(sin(dot(tileCoord, float2(12.9898, 78.233))) * 43758.5453);
    output.waterPhaseOffset = rand;
    return output;
}
)";
}

const char* GetTerrainPSSource() {
    static const std::string source = []() {
        int waterLayer = textures::GetLayerForTextureId(
            blocktextures::GetBlockTextureCatalog(),
            "minecraft:blocks/water_still"
        );
        
        return std::string(R"(
#include "ColorMonochrome.hlsl"

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
    float  waterAnimPhase;  // Continuous time-based phase for water animation
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
    float dist : TEXCOORD1;
    nointerpolation float waterPhaseOffset : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.texcoord.xy;
    float layer = input.texcoord.z;

    // Water layer resolved from the runtime texture catalog.
    const int WATER_BASE_LAYER = )") + std::to_string(waterLayer) + R"(;

    float4 texColor;
    if ((int)layer == WATER_BASE_LAYER) {
        texColor = blockTextures.Sample(samplerState, input.texcoord);
        // Make water more transparent.
        texColor.a = 0.7f;
    } else {
        texColor = blockTextures.Sample(samplerState, input.texcoord);
    }
    
    // Binary alpha test (cutout): discard pixels below threshold (e.g. leaves, grass)
    static const float ALPHA_CUTOFF = 0.5;
    clip(texColor.a - ALPHA_CUTOFF);
    
    float3 mappedColor = texColor.rgb;

    float fogFactor = saturate((input.dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 fogLinear = sRGBToLinear(fogColor);
    float3 finalColorLinear = lerp(mappedColor, fogLinear, fogFactor);
    
    // Output linear; render target is sRGB so GPU converts on write for readback / LUT
    return float4(finalColorLinear, texColor.a);
}
)";
    }();
    return source.c_str();
}

ASCIIgL::UniformBufferLayout GetTerrainPSUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Add("cameraPos", ASCIIgL::UniformType::Float3)
        .Add("fogParams", ASCIIgL::UniformType::Float4)
        .Add("fogColor", ASCIIgL::UniformType::Float3)
        .Add("waterAnimPhase", ASCIIgL::UniformType::Float)
        .Build();
}

} // namespace TerrainShaders
