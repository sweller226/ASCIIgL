#pragma once

#include <array>
#include <cstdint>
#include <memory>
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

    virtual ~Palette() = default;

    /// Deep copy; returns MonochromePalette when called on MonochromePalette.
    virtual std::unique_ptr<Palette> clone() const;

    // Get color in 0-255 range (for rendering, shaders, etc.)
    glm::ivec3 GetRGB(unsigned int idx) const;
    
    // Get color in 0-15 range (for terminal character attributes)
    glm::ivec3 GetRGB16(unsigned int idx) const;
    
    // Get normalized color in 0.0-1.0 range (for shaders)
    glm::vec3 GetRGBNormalized(unsigned int idx) const;
    
    // Get luminance (perceptual brightness, Rec. 709)
    float GetLuminance(unsigned int idx) const;

    /// Index of the palette entry with smallest luminance (darkest).
    unsigned int GetMinLumIdx() const;
    /// Index of the palette entry with largest luminance (brightest).
    unsigned int GetMaxLumIdx() const;
    
    // Terminal attribute methods (use hex value for console)
    unsigned short GetHex(unsigned int idx) const;
    unsigned short GetFgColor(unsigned int idx) const;
    unsigned short GetBgColor(unsigned int idx) const;
};

// Gradient palette (dark to light) along a hue direction; inherits from Palette.
// Stores darkL, lightL, and hueDir for inspection or regeneration.
class MonochromePalette : public Palette {
public:
    // Create a gradient palette from dark to light luminance with hue direction (hueDir in 0-255).
    // Colors are interpolated with gamma correction across 16 entries.
    MonochromePalette(float darkL, float lightL, const glm::ivec3& hueDir);

    float GetDarkL() const { return _darkL; }
    float GetLightL() const { return _lightL; }
    const glm::ivec3& GetHueDir() const { return _hueDir; }

    std::unique_ptr<Palette> clone() const override;

private:
    float _darkL = 0.0f;
    float _lightL = 0.0f;
    glm::ivec3 _hueDir = glm::ivec3(0);
};

} // namespace ASCIIgL