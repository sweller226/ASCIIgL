#include <ASCIIgL/renderer/HLSLIncludes.hpp>

namespace ASCIIgL {
namespace HLSLIncludes {

// ColorMonochrome.hlsl - sRGB/linear conversion and luminance-based monochrome gradient (matches Renderer LUT).
// Use direction (linearEnd - linearStart) so gradient aligns with the LUT segment.
const char* const COLOR_MONOCHROME_SOURCE = R"HLSL(
static const float3 REC709 = float3(0.2126, 0.7152, 0.0722);

float3 sRGBToLinear(float3 c)
{
    float3 lo = c / 12.92;
    float3 hi = pow(max((c + 0.055) / 1.055, 0.0), 2.4);
    return lerp(lo, hi, step(0.04045, c));
}

float3 linearToSRGB(float3 c)
{
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(max(c, 0.0), 1.0/2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, c));
}

// Maps luminance to linear RGB along the gradient segment (linearStart -> linearEnd).
// Direction = normalize(linearEnd - linearStart) to match monochrome LUT.
float3 LinearLuminanceToGradientRGB(float luminance, float3 linearStart, float3 linearEnd)
{
    float minLum = dot(linearStart, REC709);
    float maxLum = dot(linearEnd, REC709);
    float L_out = lerp(minLum, maxLum, luminance);
    float3 gradientDir = normalize(linearEnd - linearStart + 1e-6);
    float lumWeight = max(dot(gradientDir, REC709), 1e-6);
    return gradientDir * (L_out / lumWeight);
}
)HLSL";

const char* ColorMonochromeFileName() {
    return "ColorMonochrome.hlsl";
}

const char* ColorMonochromeSource() {
    return COLOR_MONOCHROME_SOURCE;
}

void AddToMap(ShaderIncludeMap& map) {
    map[ColorMonochromeFileName()] = ColorMonochromeSource();
}

} // namespace HLSLIncludes
} // namespace ASCIIgL
