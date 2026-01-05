#include <ASCIIgL/renderer/Palette.hpp>

namespace ASCIIgL {

Palette::Palette() {
    // 16-color palette:
    // - Index 0-15: Standard 16 colors (mapped to attributes 0x0-0xF)
    // - Foreground/background overrides are set to match index 0 in Windows Terminal
    
    entries = {{
        //   rgb                  hex    // Description
        { {0, 0, 0},            0x0 },   // Index 0: black (attribute 0x0)
        { {0, 0, 8},            0x1 },   // Index 1: blue (attribute 0x1)
        { {0, 8, 0},            0x2 },   // Index 2: green (attribute 0x2)
        { {0, 8, 8},            0x3 },   // Index 3: cyan (attribute 0x3)
        { {8, 0, 0},            0x4 },   // Index 4: red (attribute 0x4)
        { {8, 0, 8},            0x5 },   // Index 5: purple/magenta (attribute 0x5)
        { {8, 8, 0},            0x6 },   // Index 6: yellow (attribute 0x6)
        { {11, 11, 11},         0x7 },   // Index 7: white/light gray (attribute 0x7)
        { {8, 8, 8},            0x8 },   // Index 8: brightBlack/dark gray (attribute 0x8)
        { {0, 0, 15},           0x9 },   // Index 9: brightBlue (attribute 0x9)
        { {0, 15, 0},           0xA },   // Index 10: brightGreen (attribute 0xA)
        { {0, 15, 15},          0xB },   // Index 11: brightCyan (attribute 0xB)
        { {15, 0, 0},           0xC },   // Index 12: brightRed (attribute 0xC)
        { {15, 0, 15},          0xD },   // Index 13: brightPurple (attribute 0xD)
        { {15, 15, 0},          0xE },   // Index 14: brightYellow (attribute 0xE)
        { {15, 15, 15},         0xF }    // Index 15: brightWhite (attribute 0xF)
    }};
}

Palette::Palette(std::array<PaletteEntry, 16> customEntries) {
    for (int i = 0; i < 16; ++i) {
        entries[i] = customEntries[i];
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