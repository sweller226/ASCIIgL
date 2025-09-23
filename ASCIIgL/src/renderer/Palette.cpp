#include <ASCIIgL/renderer/Palette.hpp>

Palette::Palette() {
    entries = {{
        //   rgb                      index  hex
        { {0.0f, 0.0f, 0.0f},           0, 0x0 }, // Black
        { {0.0f, 0.0f, 0.5f},           1, 0x1 }, // Dark Blue
        { {0.0f, 0.5f, 0.0f},           2, 0x2 }, // Dark Green
        { {0.0f, 0.5f, 0.5f},           3, 0x3 }, // Dark Cyan
        { {0.5f, 0.0f, 0.0f},           4, 0x4 }, // Dark Red
        { {0.5f, 0.0f, 0.5f},           5, 0x5 }, // Dark Magenta
        { {0.5f, 0.4f, 0.0f},           6, 0x6 }, // Brown (Dark Yellow)
        { {0.75f, 0.75f, 0.75f},        7, 0x7 }, // Light Gray
        { {0.5f, 0.5f, 0.5f},           8, 0x8 }, // Dark Gray
        { {0.0f, 0.0f, 1.0f},           9, 0x9 }, // Blue
        { {0.0f, 1.0f, 0.0f},          10, 0xA }, // Green
        { {0.0f, 1.0f, 1.0f},          11, 0xB }, // Cyan
        { {1.0f, 0.0f, 0.0f},          12, 0xC }, // Red
        { {1.0f, 0.0f, 1.0f},          13, 0xD }, // Magenta
        { {1.0f, 1.0f, 0.0f},          14, 0xE }, // Yellow
        { {1.0f, 1.0f, 1.0f},          15, 0xF }  // White
    }};
}

Palette::Palette(std::array<PaletteEntry, COLOR_COUNT> customEntries) : entries(customEntries) {
    // No additional logic needed; entries are initialized via member initializer list
}

glm::vec3 Palette::GetRGB(unsigned int idx) const {
    if (idx >= entries.size()) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return entries[idx].rgb;
}

unsigned short Palette::GetHex(unsigned int idx) const {
    if (idx >= entries.size()) {
        return 0x0;
    }
    return entries[idx].hex;
}

unsigned short Palette::GetFgColor(unsigned int idx) const {
    if (idx >= entries.size()) {
        return 0x0;
    }
    return entries[idx].hex;
}

unsigned short Palette::GetBgColor(unsigned int idx) const {
    if (idx >= entries.size()) {
        return 0x0;
    }
    return (entries[idx].hex << 4);
}