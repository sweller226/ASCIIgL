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

    glm::ivec3 GetRGB(unsigned int idx) const;
    unsigned short GetHex(unsigned int idx) const;
    unsigned short GetFgColor(unsigned int idx) const;
    unsigned short GetBgColor(unsigned int idx) const;
};

} // namespace ASCIIgL