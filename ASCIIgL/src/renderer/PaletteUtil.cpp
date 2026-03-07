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
        
        // Linear luminance (vec3 only)
        float LinearRGB_Luminance(const glm::vec3& c) {
            return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
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
    }
}