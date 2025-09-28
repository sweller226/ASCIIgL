#include <ASCIIgL/renderer/Palette.hpp>

Palette::Palette() {
    entries = {{
        //   rgb                      hex
        { {0.0f, 0.0f, 0.0f},           0x0 }, // black
        { {0.5f, 0.0f, 0.0f},           0x1 }, // red
        { {0.0f, 0.5f, 0.0f},           0x2 }, // green
        { {0.5f, 0.5f, 0.0f},           0x3 }, // yellow
        { {0.0f, 0.0f, 0.5f},           0x4 }, // blue
        { {0.5f, 0.0f, 0.5f},           0x5 }, // purple (magenta)
        { {0.0f, 0.5f, 0.5f},           0x6 }, // cyan
        { {0.75f, 0.75f, 0.75f},       0x7 }, // white (light gray)
        // bright variants (8..15)
        { {0.5f, 0.5f, 0.5f},           0x8 }, // brightBlack / dark gray
        { {1.0f, 0.0f, 0.0f},           0x9 }, // brightRed
        { {0.0f, 1.0f, 0.0f},           0xA }, // brightGreen
        { {1.0f, 1.0f, 0.0f},           0xB }, // brightYellow
        { {0.0f, 0.0f, 1.0f},           0xC }, // brightBlue
        { {1.0f, 0.0f, 1.0f},           0xD }, // brightPurple
        { {0.0f, 1.0f, 1.0f},           0xE }, // brightCyan
        { {1.0f, 1.0f, 1.0f},           0xF }  // brightWhite
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