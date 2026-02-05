#pragma once

#include <array>
#include <cstdint>
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

// A palette entry with cached color representations
struct PaletteEntry {
    glm::ivec3 rgb;          // RGB in [0,255] range
    glm::ivec3 rgb16;        // RGB in [0,15] range (for terminal attributes)
    glm::vec3  normalized;   // RGB in [0.0,1.0] range (for shaders)
    float      luminance;    // Perceptual brightness (Rec. 709)
    unsigned short hex;      // Hex value used in COLOR enum

    // Default constructor
    PaletteEntry();

    // Constructor from 0-255 RGB - computes all cached values
    PaletteEntry(const glm::ivec3& rgbVal, unsigned short hexVal);

    // Constructor with separate RGB values
    PaletteEntry(int r, int g, int b, unsigned short hexVal);
};

class Palette {
public:
    static constexpr unsigned int COLOR_COUNT = 16;
    std::array<PaletteEntry, COLOR_COUNT> entries;

    Palette();
    Palette(std::array<PaletteEntry, 16> customEntries);
    
    // Create a gradient palette from dark to light luminance with hue direction (colors in 0-255 range)
    // Colors are interpolated with gamma correction across 16 entries
    Palette(float darkL, float lightL, const glm::ivec3& hueDir);

    // Get color in 0-255 range (for rendering, shaders, etc.)
    glm::ivec3 GetRGB(unsigned int idx) const;
    
    // Get color in 0-15 range (for terminal character attributes)
    glm::ivec3 GetRGB16(unsigned int idx) const;
    
    // Get normalized color in 0.0-1.0 range (for shaders)
    glm::vec3 GetRGBNormalized(unsigned int idx) const;
    
    // Get luminance (perceptual brightness, Rec. 709)
    float GetLuminance(unsigned int idx) const;
    
    // Terminal attribute methods (use hex value for console)
    unsigned short GetHex(unsigned int idx) const;
    unsigned short GetFgColor(unsigned int idx) const;
    unsigned short GetBgColor(unsigned int idx) const;
};

} // namespace ASCIIgL