#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// A palette entry: RGB color and its console enum hex mapping
struct PaletteEntry {
    glm::ivec3 rgb;      // RGB in [0,255] range
    unsigned short  hex;      // Hex value used in COLOR enum
};

class Palette {
public:
    static constexpr unsigned int COLOR_COUNT = 16;
    std::array<PaletteEntry, COLOR_COUNT> entries;

    Palette();
    Palette(std::array<PaletteEntry, 15> customEntries);

    glm::ivec3 GetRGB(unsigned int idx) const;
    unsigned short GetHex(unsigned int idx) const;
    unsigned short GetFgColor(unsigned int idx) const;
    unsigned short GetBgColor(unsigned int idx) const;
};