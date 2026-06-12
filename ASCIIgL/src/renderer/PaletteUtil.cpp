#include <ASCIIgL/renderer/PaletteUtil.hpp>

namespace ASCIIgL {

namespace PaletteUtil {

float sRGB1ToLinear1(float s) {
    return (s <= 0.04045f)
        ? (s / 12.92f)
        : std::pow((s + 0.055f) / 1.055f, 2.4f);
}

glm::vec3 sRGB1ToLinear1(const glm::vec3& c) {
    return glm::vec3(
        sRGB1ToLinear1(c.r),
        sRGB1ToLinear1(c.g),
        sRGB1ToLinear1(c.b)
    );
}

// sRGB → Linear (float 0–1)
float sRGB255ToLinear1(float s) {
    s /= 255.0f;
    return sRGB1ToLinear1(s);
}

// sRGB → Linear (ivec3 0–255)
glm::vec3 sRGB255ToLinear1(const glm::ivec3& c) {
    return glm::vec3(
        sRGB255ToLinear1(c.r),
        sRGB255ToLinear1(c.g),
        sRGB255ToLinear1(c.b)
    );
}

float LinearRGB_Luminance(const glm::vec3& c) {
    return glm::dot(c, Rec709WeightsVec());
}

float sRGB255_Luminance(const glm::ivec3& c) {
    glm::vec3 lin = sRGB255ToLinear1(c);
    return LinearRGB_Luminance(lin);
}

float sRGB1_Luminance(const glm::vec3& c) {
    glm::vec3 lin = sRGB1ToLinear1(c);
    return LinearRGB_Luminance(lin);
}

// Linear → sRGB (float 0–1 → 0–255)
float Linear1ToSrgb255(float c) {
    float s = (c <= 0.0031308f)
        ? (12.92f * c)
        : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);

    return glm::clamp(s * 255.0f, 0.0f, 255.0f);
}

// Linear → sRGB (vec3 0–1 → 0–255)
glm::vec3 Linear1ToSrgb255(const glm::vec3& c) {
    return glm::vec3(
        Linear1ToSrgb255(c.r),
        Linear1ToSrgb255(c.g),
        Linear1ToSrgb255(c.b)
    );
}

glm::vec4 sRGB1ToLinear1(const glm::vec4& c) {
    return glm::vec4(
        sRGB1ToLinear1(c.r),
        sRGB1ToLinear1(c.g),
        sRGB1ToLinear1(c.b),
        c.a  // alpha is always linear, never gamma-encoded
    );
}

glm::vec4 sRGB255ToLinear1(const glm::ivec4& c) {
    return glm::vec4(
        sRGB255ToLinear1(c.r),
        sRGB255ToLinear1(c.g),
        sRGB255ToLinear1(c.b),
        c.a / 255.0f
    );
}

glm::vec4 Linear1ToSrgb255(const glm::vec4& c) {
    glm::vec3 rgb = Linear1ToSrgb255(glm::vec3(c));
    return glm::vec4(rgb, glm::clamp(c.a * 255.0f, 0.0f, 255.0f));
}

}

}