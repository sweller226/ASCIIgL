#pragma once

#include <glm/glm.hpp>

namespace ASCIIgL {

namespace PaletteUtil {
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
    // Linear → sRGB
    // -------------------------

    // Convert linear channel (0–1) → sRGB 0–255
    float Linear1ToSrgb255(float c);

    // Convert linear RGB (0–1) → sRGB 0–255
    glm::vec3 Linear1ToSrgb255(const glm::vec3& c);
}

} // namespace ASCIIgL

