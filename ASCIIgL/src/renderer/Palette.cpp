#include <ASCIIgL/renderer/Palette.hpp>

namespace ASCIIgL {

Palette::Palette() {
    entries = {{
        //   rgb                  hex
        { {0, 0, 0},            0x0 }, // black
        { {8, 0, 0},            0x1 }, // red
        { {0, 8, 0},            0x2 }, // green
        { {8, 8, 0},            0x3 }, // yellow
        { {0, 0, 8},            0x4 }, // blue
        { {8, 0, 8},            0x5 }, // purple (magenta)
        { {0, 8, 8},            0x6 }, // cyan
        { {11, 11, 11},         0x7 }, // white (light gray)
        // bright variants (8..15)
        { {8, 8, 8},            0x8 }, // brightBlack / dark gray
        { {15, 0, 0},           0x9 }, // brightRed
        { {0, 15, 0},           0xA }, // brightGreen
        { {15, 15, 0},          0xB }, // brightYellow
        { {0, 0, 15},           0xC }, // brightBlue
        { {15, 0, 15},          0xD }, // brightPurple
        { {0, 15, 15},          0xE }, // brightCyan
        { {15, 15, 15},         0xF }  // brightWhite
    }};
}

Palette::Palette(std::array<PaletteEntry, 15> customEntries) {
    // Black is always required at color 0
    entries[0] = { {0, 0, 0}, 0x0 };
    
    // Copy the 15 custom entries to positions 1-15
    for (int i = 0; i < 15; ++i) {
        entries[i + 1] = customEntries[i];
    }
}

glm::ivec3 Palette::GetRGB(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::ivec3(0);
    return entries[idx].rgb;
}

unsigned short Palette::GetHex(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return 0x0;
    return static_cast<unsigned short>(entries[idx].hex & 0xF);
}

unsigned short Palette::GetFgColor(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return 0x0;
    return static_cast<unsigned short>(entries[idx].hex & 0xF);
}

unsigned short Palette::GetBgColor(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return 0x0;
    return static_cast<unsigned short>((entries[idx].hex & 0xF) << 4);
}

} // namespace ASCIIgL