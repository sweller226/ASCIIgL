#include <ASCIIgL/renderer/Palette.hpp>

Palette::Palette() {
    entries = {{
        //   rgb                      index  hex
        { {0.0f, 0.0f, 0.0f},           0, 0x0 }, // black
        { {0.5f, 0.0f, 0.0f},           1, 0x1 }, // red
        { {0.0f, 0.5f, 0.0f},           2, 0x2 }, // green
        { {0.5f, 0.5f, 0.0f},           3, 0x3 }, // yellow
        { {0.0f, 0.0f, 0.5f},           4, 0x4 }, // blue
        { {0.5f, 0.0f, 0.5f},           5, 0x5 }, // purple (magenta)
        { {0.0f, 0.5f, 0.5f},           6, 0x6 }, // cyan
        { {0.75f, 0.75f, 0.75f},        7, 0x7 }, // white (light gray)
        // bright variants (8..15)
        { {0.5f, 0.5f, 0.5f},           8, 0x8 }, // brightBlack / dark gray
        { {1.0f, 0.0f, 0.0f},           9, 0x9 }, // brightRed
        { {0.0f, 1.0f, 0.0f},          10, 0xA }, // brightGreen
        { {1.0f, 1.0f, 0.0f},          11, 0xB }, // brightYellow
        { {0.0f, 0.0f, 1.0f},          12, 0xC }, // brightBlue
        { {1.0f, 0.0f, 1.0f},          13, 0xD }, // brightPurple
        { {0.0f, 1.0f, 1.0f},          14, 0xE }, // brightCyan
        { {1.0f, 1.0f, 1.0f},          15, 0xF }  // brightWhite
    }};
}

Palette::Palette(std::array<PaletteEntry, COLOR_COUNT> customEntries) : entries(customEntries) {
    // No additional logic needed; entries are initialized via member initializer list
}

glm::vec3 Palette::GetRGB(unsigned int idx) const {
    if (idx >= entries.size()) return glm::vec3(0.0f);
    return entries[idx].rgb;
}

unsigned short Palette::GetHex(unsigned int idx) const {
    if (idx >= entries.size()) return 0x0;
    return static_cast<unsigned short>(entries[idx].hex & 0xF);
}

unsigned short Palette::GetFgColor(unsigned int idx) const {
    if (idx >= entries.size()) return 0x0;
    return static_cast<unsigned short>(entries[idx].hex & 0xF);
}

unsigned short Palette::GetBgColor(unsigned int idx) const {
    if (idx >= entries.size()) return 0x0;
    return static_cast<unsigned short>((entries[idx].hex & 0xF) << 4);
}