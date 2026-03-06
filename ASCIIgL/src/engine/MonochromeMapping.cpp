#include <ASCIIgL/engine/MonochromeMapping.hpp>
#include <ASCIIgL/renderer/PaletteUtil.hpp>

#include <algorithm>
#include <cmath>

namespace ASCIIgL {

namespace {

constexpr float REC709_R = 0.2126f;
constexpr float REC709_G = 0.7152f;
constexpr float REC709_B = 0.0722f;

inline glm::vec3 LinearLuminanceToGradientRGB(float luminance,
                                              const glm::vec3& linearStart,
                                              const glm::vec3& linearEnd)
{
    const glm::vec3 REC709(REC709_R, REC709_G, REC709_B);

    float minLum = glm::dot(linearStart, REC709);
    float maxLum = glm::dot(linearEnd,   REC709);

    float t = std::clamp(luminance, 0.0f, 1.0f);
    float L_out = minLum + (maxLum - minLum) * t;

    glm::vec3 gradientDir = glm::normalize(linearEnd - linearStart + glm::vec3(1e-6f));
    float lumWeight = std::max(glm::dot(gradientDir, REC709), 1e-6f);
    return gradientDir * (L_out / lumWeight);
}

} // namespace

void ApplyMonochromeMappingRGBA8(uint8_t* rgbaData, int width, int height, const MonochromeMapping& mapping) {
    if (!rgbaData || width <= 0 || height <= 0 || !mapping.enabled) {
        return;
    }

    glm::vec3 hueLin = PaletteUtil::sRGB255ToLinear1(mapping.hueDir);
    if (glm::length(hueLin) < 1e-6f) {
        hueLin = glm::vec3(1.0f);
    }
    glm::vec3 hue = glm::normalize(hueLin);

    glm::vec3 linearStart = hue * mapping.darkL;
    glm::vec3 linearEnd   = hue * mapping.lightL;

    const int pixelCount = width * height;
    for (int i = 0; i < pixelCount; ++i) {
        uint8_t* px = rgbaData + i * 4;
        glm::ivec3 rgb(px[0], px[1], px[2]);
        uint8_t a = px[3];

        glm::vec3 lin = PaletteUtil::sRGB255ToLinear1(rgb);
        float luminance = PaletteUtil::LinearRGB_Luminance(lin);

        glm::vec3 monoLin = LinearLuminanceToGradientRGB(luminance, linearStart, linearEnd);
        glm::vec3 monoSRGBf = PaletteUtil::Linear1ToSrgb255(monoLin);

        px[0] = static_cast<uint8_t>(monoSRGBf.r + 0.5f);
        px[1] = static_cast<uint8_t>(monoSRGBf.g + 0.5f);
        px[2] = static_cast<uint8_t>(monoSRGBf.b + 0.5f);
        px[3] = a;
    }
}

} // namespace ASCIIgL

