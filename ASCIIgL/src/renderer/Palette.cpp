#include <ASCIIgL/renderer/Palette.hpp>

#include <cmath>

namespace ASCIIgL {

namespace PaletteUtil {

    // sRGB → Linear (float 0–1)
    float sRGB255ToLinear1(float s) {
        s /= 255.0f;
        return (s <= 0.04045f)
            ? (s / 12.92f)
            : std::pow((s + 0.055f) / 1.055f, 2.4f);
    }

    // sRGB → Linear (ivec3 0–255)
    glm::vec3 sRGB255ToLinear1(const glm::ivec3& c) {
        return glm::vec3(
            sRGB255ToLinear1(c.r),
            sRGB255ToLinear1(c.g),
            sRGB255ToLinear1(c.b)
        );
    }

    // Luminance (sRGB input)
    float sRGB255_Luminance(const glm::ivec3& c) {
        glm::vec3 lin = sRGB255ToLinear1(c);
        return 0.2126f * lin.r + 0.7152f * lin.g + 0.0722f * lin.b;
    }

    // Linear luminance (vec3 only)
    float LinearRGB_Luminance(const glm::vec3& c) {
        return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    }

    // Linear → sRGB (float 0–1 → 0–255)
    float Linear1ToSrgb255(float c) {
        float s = (c <= 0.0031308f)
            ? (12.92f * c)
            : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);

        return glm::clamp(s * 255.0f, 0.0f, 255.0f);
    }

    // Linear → sRGB (vec3 0–1 → 0–255)
    glm::vec3 Linear1ToSrgb255(const glm::vec3& c) {
        return glm::vec3(
            Linear1ToSrgb255(c.r),
            Linear1ToSrgb255(c.g),
            Linear1ToSrgb255(c.b)
        );
    }
}

PaletteEntry::PaletteEntry(const glm::ivec3& rgbVal, unsigned short hexVal)
    : rgb(rgbVal)
    , rgb16(rgbVal.r / 17, rgbVal.g / 17, rgbVal.b / 17)
    , normalized(glm::vec3(rgbVal) / 255.0f)
    , luminance(PaletteUtil::sRGB255_Luminance(glm::vec3(rgbVal)))
    , hex(hexVal) {}

PaletteEntry::PaletteEntry() : rgb(0, 0, 0), rgb16(0, 0, 0), normalized(0.0f), luminance(0.0f), hex(0) {}

PaletteEntry::PaletteEntry(int r, int g, int b, unsigned short hexVal)
    : PaletteEntry(glm::ivec3(r, g, b), hexVal) {}

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

Palette::Palette(float darkL, float lightL, const glm::ivec3& hueDir)
{
    glm::vec3 hueDirLinear = PaletteUtil::sRGB255ToLinear1(hueDir);

    constexpr float gamma = 2.2f;
    constexpr float invGamma = 1.0f / gamma;

    glm::vec3 hue = glm::normalize(hueDirLinear);

    for (int i = 0; i < 16; ++i) {
        float t = static_cast<float>(i) / 15.0f;
        float L = std::pow(t, invGamma);

        float Y = glm::mix(darkL, lightL, L);

        glm::vec3 colorLin = hue * Y;

        glm::ivec3 color = PaletteUtil::Linear1ToSrgb255(colorLin);

        entries[i] = PaletteEntry(color, static_cast<unsigned short>(i));
    }
}

glm::ivec3 Palette::GetRGB(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::ivec3(0);
    return entries[idx].rgb;  // Cached 0-255 range
}

glm::ivec3 Palette::GetRGB16(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::ivec3(0);
    return entries[idx].rgb16;  // Cached 0-15 range
}

glm::vec3 Palette::GetRGBNormalized(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return glm::vec3(0.0f);
    return entries[idx].normalized;  // Cached 0.0-1.0 range
}

float Palette::GetLuminance(unsigned int idx) const {
    if (idx >= COLOR_COUNT) return 0.0f;
    return entries[idx].luminance;  // Cached luminance (Rec. 709)
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