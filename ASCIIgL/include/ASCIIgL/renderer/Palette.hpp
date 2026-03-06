#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <ASCIIgL/renderer/PaletteUtil.hpp>

namespace ASCIIgL {

class Texture;
class TextureArray;

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

    // Default palette (classic 16-color terminal palette).
    Palette();

    // Build palette from explicit textures and texture arrays with weights.
    Palette(
        const std::vector<std::pair<float, std::shared_ptr<Texture>>>& textures,
        const std::vector<std::pair<float, std::shared_ptr<TextureArray>>>& textureArrays
    );

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

private:
    void SetDefaultEntries();
};

// Gradient palette (dark to light) along a hue direction; inherits from Palette.
// Stores darkL, lightL, and hueDir for inspection or regeneration.
class MonochromePalette : public Palette {
public:
    // Create a gradient palette from dark to light luminance with hue direction (hueDir in 0-255).
    // Colors are interpolated with gamma correction across 16 entries.
    MonochromePalette(float darkL, float lightL, const glm::ivec3& hueDir);
    MonochromePalette(std::array<PaletteEntry, 16> customEntries);

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