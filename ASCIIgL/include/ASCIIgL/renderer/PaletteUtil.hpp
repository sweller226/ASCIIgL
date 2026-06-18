#pragma once

#include <glm/glm.hpp>

namespace ASCIIgL {

namespace PaletteUtil {
    // Rec. 709 linear RGB luminance weights
    inline constexpr float Rec709R = 0.2126f;
    inline constexpr float Rec709G = 0.7152f;
    inline constexpr float Rec709B = 0.0722f;

    inline glm::vec3 Rec709WeightsVec() {
        return glm::vec3(Rec709R, Rec709G, Rec709B);
    }

    // -------------------------
    // sRGB → Linear
    // -------------------------

    // Convert a single sRGB channel (0–255) to linear (0–1)
    float sRGB255ToLinear1(float s);
    glm::vec3 sRGB1ToLinear1(const glm::vec3& c);

    // Convert sRGB 0–255 → linear 0–1
    glm::vec3 sRGB255ToLinear1(const glm::ivec3& c);

    // -------------------------
    // Luminance
    // -------------------------

    // Luminance from sRGB 0–255
    float sRGB255_Luminance(const glm::ivec3& c);
    float sRGB1_Luminance(const glm::vec3& c);

    // Luminance from linear RGB 0–1
    float LinearRGB_Luminance(const glm::vec3& c);

    // -------------------------
    // Oklab (perceptual color space, linear RGB 0–1 input)
    // -------------------------

    glm::vec3 Linear1ToOklab(const glm::vec3& c);

    // -------------------------
    // Linear → sRGB
    // -------------------------

    // Convert linear channel (0–1) → sRGB 0–255
    float Linear1ToSrgb255(float c);

    // Convert linear RGB (0–1) → sRGB 0–255
    glm::vec3 Linear1ToSrgb255(const glm::vec3& c);

    // -------------------------
    // sRGB → Linear (RGBA)
    // -------------------------

    glm::vec4 sRGB1ToLinear1(const glm::vec4& c);
    glm::vec4 sRGB255ToLinear1(const glm::ivec4& c);

    // -------------------------
    // Linear → sRGB (RGBA)
    // -------------------------

    glm::vec4 Linear1ToSrgb255(const glm::vec4& c);
}

} // namespace ASCIIgL

