#include <ASCIICraft/rendering/Shaders.hpp>

namespace ASCIICraft {

namespace TerrainShaders {

const char* GetTerrainVSSource() {
    // Same as texture array vertex shader - passes position + UVLayer
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;   // Start color of gradient (dark)
    float4 gradientEnd;     // End color of gradient (bright)
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 texcoord : TEXCOORD0;  // UV + Layer index
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;  // UV + Layer passed to pixel shader
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

const char* GetTerrainPSSource() {
    return R"(
Texture2DArray blockTextures : register(t0);
SamplerState samplerState : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 gradientStart;   // Start color of gradient (dark), alpha = unused
    float4 gradientEnd;     // End color of gradient (bright), alpha = unused
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;  // UV.xy + Layer.z
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Sample the texture array
    float4 texColor = blockTextures.Sample(samplerState, input.texcoord);
    
    // Compute luminance using standard coefficients (Rec. 709)
    float luminance = dot(texColor.rgb, float3(0.2126, 0.7152, 0.0722));
    
    // === Monochromatic gradient mapping (no hue drift) ===
    // Instead of lerping RGB channels independently (which causes hue drift),
    // we interpolate brightness along the palette's hue direction.
    
    // Extract hue direction from the bright end of the gradient
    float3 hue = normalize(gradientEnd.rgb + 0.0001); // Avoid div by zero
    
    // Compute brightness range from gradient endpoints
    float minBrightness = length(gradientStart.rgb);
    float maxBrightness = length(gradientEnd.rgb);
    
    // Map luminance to brightness along the hue direction
    float brightness = lerp(minBrightness, maxBrightness, luminance);
    
    // Reconstruct color = hue direction * brightness
    float3 mappedColor = hue * brightness;
    
    // Preserve original alpha
    return float4(mappedColor, texColor.a);
}
)";
}

ASCIIgL::UniformBufferLayout GetUniformLayout() {
    return ASCIIgL::UniformBufferLayout::Builder()
        .Add("mvp", ASCIIgL::UniformType::Mat4)
        .Add("gradientStart", ASCIIgL::UniformType::Float4)
        .Add("gradientEnd", ASCIIgL::UniformType::Float4)
        .Build();
}

} // namespace TerrainShaders

} // namespace ASCIICraft
