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
    nointerpolation float  waterPhaseOffset : TEXCOORD3; // Per-block random phase offset for water (no interpolation)
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    output.dist = distance(input.position, cameraPos);
    output.light = input.light;

    // Compute a deterministic per-block phase offset from snapped world XZ position.
    float2 tileCoord = floor(input.position.xz);
    float rand = frac(sin(dot(tileCoord, float2(12.9898, 78.233))) * 43758.5453);
    output.waterPhaseOffset = rand;
    return output;
}
)";
}

const char* GetTerrainPSSource() {
    return R"(
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
    float3 texcoord : TEXCOORD0;  // UV.xy + Layer.z
    float dist : TEXCOORD1;       // Distance from camera
    float light : TEXCOORD2;      // Per-vertex directional light
    nointerpolation float waterPhaseOffset : TEXCOORD3; // Per-block phase offset (no interpolation)
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.texcoord.xy;
    float layer = input.texcoord.z;

    // Water animation: identify water base layer and switch between 5 atlas frames (no lerp)
    // Atlas coordinates for water frames: (11,12), (12,12), (13,12), (12,13), (13,13)
    static const int ATLAS_SIZE = 16;
    static const int WATER_FRAME_COUNT = 5;
    static const int WATER_LAYERS[WATER_FRAME_COUNT] = {
        12 * ATLAS_SIZE + 11,  // (11,12)
        12 * ATLAS_SIZE + 12,  // (12,12)
        12 * ATLAS_SIZE + 13,  // (13,12)
        13 * ATLAS_SIZE + 12,  // (12,13)
        13 * ATLAS_SIZE + 13   // (13,13)
    };
    const int WATER_BASE_LAYER = WATER_LAYERS[0];

    float4 texColor;
    if ((int)layer == WATER_BASE_LAYER) {
        // Compute a single frame index from global phase + per-block offset
        float localPhase = waterAnimPhase + input.waterPhaseOffset;
        float framePos   = frac(localPhase) * WATER_FRAME_COUNT; // [0, WATER_FRAME_COUNT)
        int frameIdx     = (int)framePos % WATER_FRAME_COUNT;

        float3 uvw = float3(uv, (float)WATER_LAYERS[frameIdx]);
        texColor = blockTextures.Sample(samplerState, uvw);

        // Make water more transparent by reducing alpha.
        texColor.a = 0.6f;
    } else {
        // Sample the texture array (already linear RGB from sRGB texture format)
        texColor = blockTextures.Sample(samplerState, input.texcoord);
    }
    
    // Binary alpha test (cutout): discard pixels below threshold (e.g. leaves, grass)
    static const float ALPHA_CUTOFF = 0.5;
    clip(texColor.a - ALPHA_CUTOFF);
    
    // Per-vertex directional lighting on already-baked monochrome texture color
    float3 mappedColor = texColor.rgb * input.light;

    // Fog (fogColor assumed sRGB; linearize for correct blend in linear space)
    float fogFactor = saturate((input.dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 fogLinear = sRGBToLinear(fogColor);
    float3 finalColorLinear = lerp(mappedColor, fogLinear, fogFactor);
    
    // Output linear; render target is sRGB so GPU converts on write for readback / LUT
    return float4(finalColorLinear, texColor.a);
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
        .Add("waterAnimPhase", ASCIIgL::UniformType::Float)
        .Build();
}

} // namespace TerrainShaders
