#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// A palette entry: RGB color and its console enum hex mapping
struct PaletteEntry {
    glm::vec3 rgb;      // RGB in [0,1] range
    uint8_t   index;    // 0-15 index (matches enum value & hex mapping)
    uint16_t  hex;      // Hex value used in COLOR enum
};

class Palette {
public:
    // 16 standard colors
    static constexpr size_t COLOR_COUNT = 16;

    // Array of palette entries
    std::array<PaletteEntry, COLOR_COUNT> entries;

    Palette() {
        // Default: IBM PC/Windows console palette (CGA/EGA/VGA compatible)
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

    // Get RGB by index (0-15)
    glm::vec3 getRGB(uint8_t idx) const {
        return entries[idx % COLOR_COUNT].rgb;
    }

    // Get hex mapping by index (0-15)
    uint16_t getHex(uint8_t idx) const {
        return entries[idx % COLOR_COUNT].hex;
    }

    uint16_t getFgColor(uint8_t idx) const {
        return entries[idx % COLOR_COUNT].hex;
    }

    uint16_t getBgColor(uint8_t idx) const {
        return (entries[idx % COLOR_COUNT].hex << 4);
    }

    // Get index by hex mapping (returns 0-15 or 0 if not found)
    uint8_t getIndexByHex(uint16_t hex) const {
        for (const auto& entry : entries) {
            if (entry.hex == (hex & 0xF)) // Only low nibble for FG
                return entry.index;
        }
        return 0;
    }
};