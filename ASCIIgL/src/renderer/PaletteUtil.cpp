#include <ASCIIgL/renderer/PaletteUtil.hpp>

#include <cmath>

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

glm::vec3 Linear1ToOklab(const glm::vec3& c) {
    const float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
    const float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
    const float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

    const float l_ = std::cbrt(l);
    const float m_ = std::cbrt(m);
    const float s_ = std::cbrt(s);

    return glm::vec3(
        0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
        1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
        0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
    );
}

glm::vec3 OklabToLinear1(const glm::vec3& c) {
    const float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
    const float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
    const float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;

    const float l = l_ * l_ * l_;
    const float m = m_ * m_ * m_;
    const float s = s_ * s_ * s_;

    return glm::vec3(
        +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s
    );
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