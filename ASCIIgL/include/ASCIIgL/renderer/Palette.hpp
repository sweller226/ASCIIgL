#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace ASCIIgL {

// A palette entry: RGB color and its console enum hex mapping
struct PaletteEntry {
    glm::ivec3 rgb;      // RGB in [0,255] range
    unsigned short hex;  // Hex value used in COLOR enum

    // Default constructor
    PaletteEntry() : rgb(0, 0, 0), hex(0) {}

    // Constructor with separate RGB values
    PaletteEntry(int r, int g, int b, unsigned short hexVal)
        : rgb(r, g, b), hex(hexVal) {}

    // Constructor with glm::ivec3
    PaletteEntry(const glm::ivec3& rgbVal, unsigned short hexVal)
        : rgb(rgbVal), hex(hexVal) {}
};

class Palette {
public:
    static constexpr unsigned int COLOR_COUNT = 16;
    std::array<PaletteEntry, COLOR_COUNT> entries;

    Palette();
    Palette(std::array<PaletteEntry, 16> customEntries);
    
    // Create a gradient palette from dark to light color (colors in 0-255 range)
    // Colors are interpolated with gamma correction across 16 entries
    Palette(const glm::ivec3& darkColor, const glm::ivec3& lightColor);

    // Get color in 0-255 range (for rendering, shaders, etc.)
    glm::ivec3 GetRGB(unsigned int idx) const;
    
    // Get color in 0-15 range (for terminal character attributes)
    glm::ivec3 GetRGB16(unsigned int idx) const;
    
    // Get normalized color in 0.0-1.0 range (for shaders)
    glm::vec3 GetRGBNormalized(unsigned int idx) const;
    
    // Terminal attribute methods (use hex value for console)
    unsigned short GetHex(unsigned int idx) const;
    unsigned short GetFgColor(unsigned int idx) const;
    unsigned short GetBgColor(unsigned int idx) const;
};

} // namespace ASCIIgL