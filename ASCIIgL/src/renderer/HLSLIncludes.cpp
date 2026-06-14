#include <ASCIIgL/renderer/HLSLIncludes.hpp>
#include <ASCIIgL/renderer/PaletteUtil.hpp>

#include <string>

namespace ASCIIgL {
namespace HLSLIncludes {

namespace {

std::string BuildColorUtilSource() {
    const std::string rec709 = "float3("
        + std::to_string(PaletteUtil::Rec709R) + ", "
        + std::to_string(PaletteUtil::Rec709G) + ", "
        + std::to_string(PaletteUtil::Rec709B) + ")";

    return std::string(R"HLSL(
static const float3 REC709 = )HLSL") + rec709 + R"HLSL(;

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
}

} // namespace

const char* ColorUtilFileName() {
    return "ColorUtil.hlsl";
}

const char* ColorUtilSource() {
    static const std::string source = BuildColorUtilSource();
    return source.c_str();
}

void AddToMap(ShaderIncludeMap& map) {
    map[ColorUtilFileName()] = ColorUtilSource();
}

} // namespace HLSLIncludes
} // namespace ASCIIgL
