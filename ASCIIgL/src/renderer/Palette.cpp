#include <ASCIIgL/renderer/Palette.hpp>

#include <cmath>

namespace ASCIIgL {

Palette::Palette() {
    // 16-color palette in 0-255 range:
    // - Index 0-15: Standard 16 colors (mapped to attributes 0x0-0xF)
    
    entries = {{
        //   rgb (0-255)              hex    // Description
        { {0, 0, 0},                0x0 },   // Index 0: black
        { {0, 0, 136},              0x1 },   // Index 1: blue
        { {0, 136, 0},              0x2 },   // Index 2: green
        { {0, 136, 136},            0x3 },   // Index 3: cyan
        { {136, 0, 0},              0x4 },   // Index 4: red
        { {136, 0, 136},            0x5 },   // Index 5: purple/magenta
        { {136, 136, 0},            0x6 },   // Index 6: yellow
        { {187, 187, 187},          0x7 },   // Index 7: white/light gray
        { {136, 136, 136},          0x8 },   // Index 8: brightBlack/dark gray
        { {0, 0, 255},              0x9 },   // Index 9: brightBlue
        { {0, 255, 0},              0xA },   // Index 10: brightGreen
        { {0, 255, 255},            0xB },   // Index 11: brightCyan
        { {255, 0, 0},              0xC },   // Index 12: brightRed
        { {255, 0, 255},            0xD },   // Index 13: brightPurple
        { {255, 255, 0},            0xE },   // Index 14: brightYellow
        { {255, 255, 255},          0xF }    // Index 15: brightWhite
    }};
}

Palette::Palette(std::array<PaletteEntry, 16> customEntries) {
    for (int i = 0; i < 16; ++i) {
        entries[i] = customEntries[i];
    }
}

Palette::Palette(const glm::ivec3& darkColor, const glm::ivec3& lightColor) {
    constexpr float gamma = 2.2f;
    constexpr float invGamma = 1.0f / gamma;

    // Extract hue direction from the light color
    glm::vec3 hue = glm::normalize(glm::vec3(lightColor));

    // Compute max brightness (length of the light color vector)
    float maxBrightness = glm::length(glm::vec3(lightColor));

    // Compute min brightness from darkColor (usually 0)
    float minBrightness = glm::length(glm::vec3(darkColor));

    for (int i = 0; i < 16; ++i) {
        float t = static_cast<float>(i) / 15.0f;

        // Perceptual brightness interpolation
        float L = std::pow(t, invGamma);

        // Interpolate brightness only (monochrome ramp)
        float brightness = glm::mix(minBrightness, maxBrightness, L);

        // Reconstruct color along the hue direction
        glm::vec3 colorF = hue * brightness;

        // Clamp and convert to integers
        glm::ivec3 color(
            static_cast<int>(glm::clamp(colorF.r, 0.0f, 255.0f)),
            static_cast<int>(glm::clamp(colorF.g, 0.0f, 255.0f)),
            static_cast<int>(glm::clamp(colorF.b, 0.0f, 255.0f))
        );

        entries[i] = PaletteEntry(color, static_cast<unsigned short>(i));
    }
}


glm::ivec3 Palette::GetRGB(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::ivec3(0);
    return entries[idx].rgb;  // Returns 0-255 range
}

glm::ivec3 Palette::GetRGB16(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::ivec3(0);
    // Convert from 0-255 to 0-15 for terminal character attributes
    return glm::ivec3(
        entries[idx].rgb.r / 17,  // 255/15 ≈ 17
        entries[idx].rgb.g / 17,
        entries[idx].rgb.b / 17
    );
}

glm::vec3 Palette::GetRGBNormalized(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::vec3(0.0f);
    // Convert from 0-255 to 0.0-1.0 for shaders
    return glm::vec3(entries[idx].rgb) / 255.0f;
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