#include <ASCIIgL/renderer/Palette.hpp>

#include <cmath>
#include <string>
#include <utility>
#include <algorithm>
#include <limits>
#include <vector>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/PaletteUtil.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>

namespace ASCIIgL {

PaletteEntry::PaletteEntry(const glm::ivec3& rgbVal, unsigned short hexVal)
    : rgb(rgbVal)
    , rgb16(rgbVal.r / 17, rgbVal.g / 17, rgbVal.b / 17)
    , normalized(glm::vec3(rgbVal) / 255.0f)
    , luminance(PaletteUtil::sRGB255_Luminance(rgbVal))
    , hex(hexVal) {}

PaletteEntry::PaletteEntry() : rgb(0, 0, 0), rgb16(0, 0, 0), normalized(0.0f), luminance(0.0f), hex(0) {}

PaletteEntry::PaletteEntry(int r, int g, int b, unsigned short hexVal)
    : PaletteEntry(glm::ivec3(r, g, b), hexVal) {}

void Palette::SetDefaultEntries() {
    // Default 16-color palette in 0-255 range:
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

Palette::Palette() {
    SetDefaultEntries();
}

Palette::Palette(
    const std::vector<std::pair<float, std::shared_ptr<Texture>>>& textures,
    const std::vector<std::pair<float, std::shared_ptr<TextureArray>>>& textureArrays,
    bool sortByLuminance)
{
    // Collect color samples from given textures/arrays, using the float as a weight.
    std::vector<glm::vec3> samples;
    samples.reserve(4096);

    constexpr int BASE_SAMPLES_PER_RESOURCE = 256;
    constexpr uint8_t ALPHA_THRESHOLD = 16;

    auto sampleTexture = [&](float weight, const std::shared_ptr<Texture>& tex) {
        if (!tex || weight <= 0.0f) return;
        int w = tex->GetWidth();
        int h = tex->GetHeight();
        if (w <= 0 || h <= 0) return;

        const uint8_t* data = tex->GetDataPtr();
        if (!data) return;

        const int area = w * h;
        int targetSamples = std::max(1, static_cast<int>(BASE_SAMPLES_PER_RESOURCE * weight));
        int stride = 1;
        if (area > targetSamples) {
            float s = std::sqrt(static_cast<float>(area) / targetSamples);
            stride = std::max(1, static_cast<int>(s));
        }

        for (int y = 0; y < h; y += stride) {
            for (int x = 0; x < w; x += stride) {
                const uint8_t* px = data + (static_cast<size_t>(y) * w + x) * 4;
                uint8_t a = px[3];
                if (a < ALPHA_THRESHOLD) continue;
                glm::ivec3 rgb(px[0], px[1], px[2]);
                samples.push_back(PaletteUtil::sRGB255ToLinear1(rgb));
            }
        }
    };

    auto sampleTextureArray = [&](float weight, const std::shared_ptr<TextureArray>& arr) {
        if (!arr || weight <= 0.0f || !arr->IsValid()) return;
        int tileSize = arr->GetTileSize();
        int layers   = arr->GetLayerCount();
        if (tileSize <= 0 || layers <= 0) return;

        const int area = tileSize * tileSize;
        int targetSamples = std::max(1, static_cast<int>(BASE_SAMPLES_PER_RESOURCE * weight));
        int stride = 1;
        if (area > targetSamples) {
            float s = std::sqrt(static_cast<float>(area) / targetSamples);
            stride = std::max(1, static_cast<int>(s));
        }

        for (int layer = 0; layer < layers; ++layer) {
            const uint8_t* data = arr->GetLayerData(layer, 0);
            if (!data) continue;

            for (int y = 0; y < tileSize; y += stride) {
                for (int x = 0; x < tileSize; x += stride) {
                    const uint8_t* px = data + (static_cast<size_t>(y) * tileSize + x) * 4;
                    uint8_t a = px[3];
                    if (a < ALPHA_THRESHOLD) continue;
                    glm::ivec3 rgb(px[0], px[1], px[2]);
                    samples.push_back(PaletteUtil::sRGB255ToLinear1(rgb));
                }
            }
        }
    };

    for (auto& entry : textures) {
        sampleTexture(entry.first, entry.second);
    }
    for (auto& entry : textureArrays) {
        sampleTextureArray(entry.first, entry.second);
    }

    constexpr int K = 16;
    if (!samples.empty()) {
        const int K_eff = std::min<int>(K, static_cast<int>(samples.size()));

        std::array<glm::vec3, K> centers{};
        for (int k = 0; k < K_eff; ++k) {
            size_t idx = static_cast<size_t>((static_cast<long long>(k) * samples.size()) / K_eff);
            if (idx >= samples.size()) idx = samples.size() - 1;
            centers[k] = samples[idx];
        }
        for (int k = K_eff; k < K; ++k) {
            centers[k] = centers[K_eff - 1];
        }

        std::vector<int> assignment(samples.size(), 0);
        const int MAX_ITERS = 10;

        for (int iter = 0; iter < MAX_ITERS; ++iter) {
            bool changed = false;

            for (size_t i = 0; i < samples.size(); ++i) {
                float bestDist = std::numeric_limits<float>::max();
                int bestK = 0;
                for (int k = 0; k < K; ++k) {
                    glm::vec3 d = samples[i] - centers[k];
                    float dist = glm::dot(d, d);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestK = k;
                    }
                }
                if (assignment[i] != bestK) {
                    assignment[i] = bestK;
                    changed = true;
                }
            }

            std::array<glm::vec3, K> sum{};
            std::array<int, K> count{};
            for (int k = 0; k < K; ++k) {
                sum[k] = glm::vec3(0.0f);
                count[k] = 0;
            }

            for (size_t i = 0; i < samples.size(); ++i) {
                int k = assignment[i];
                sum[k] += samples[i];
                ++count[k];
            }

            for (int k = 0; k < K; ++k) {
                if (count[k] > 0) {
                    centers[k] = sum[k] / static_cast<float>(count[k]);
                }
            }

            if (!changed) break;
        }

        for (int k = 0; k < K; ++k) {
            glm::ivec3 rgb = PaletteUtil::Linear1ToSrgb255(centers[k]);
            entries[k] = PaletteEntry(rgb, static_cast<unsigned short>(k));
        }
    } else {
        // No samples; fall back to default palette.
        SetDefaultEntries();
    }

    if (sortByLuminance) {
        std::sort(entries.begin(), entries.end(),
                  [](const PaletteEntry& a, const PaletteEntry& b) {
                      return a.luminance < b.luminance;
                  });
    }
}

Palette::Palette(std::array<PaletteEntry, 16> customEntries) {
    for (int i = 0; i < 16; ++i) {
        entries[i] = customEntries[i];
    }
}

std::unique_ptr<Palette> Palette::clone() const {
    return std::make_unique<Palette>(entries);
}

MonochromePalette::MonochromePalette(float darkL, float lightL, const glm::ivec3& hueDir)
{
    if (darkL > lightL) std::swap(darkL, lightL);
    _darkL = darkL;
    _lightL = lightL;
    _hueDir = hueDir;
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

    // Ensure entries are sorted by luminance (ascending)
    std::sort(entries.begin(), entries.end(),
              [](const PaletteEntry& a, const PaletteEntry& b) {
                  return a.luminance < b.luminance;
              });

    Logger::Debug("[MonochromePalette] Gradient palette (darkL=" + std::to_string(_darkL) + " lightL=" + std::to_string(_lightL) + " hueDir=(" +
        std::to_string(_hueDir.r) + "," + std::to_string(_hueDir.g) + "," + std::to_string(_hueDir.b) + ")");
    for (int i = 0; i < 16; ++i) {
        const glm::ivec3& rgb = entries[i].rgb;
        float lum = entries[i].luminance;
        Logger::Debug("[MonochromePalette]   index " + std::to_string(i) + " => RGB(" +
            std::to_string(rgb.r) + ", " + std::to_string(rgb.g) + ", " + std::to_string(rgb.b) + ") luminance " + std::to_string(lum));
    }
}

void MonochromePalette::InferParamsFromEntries() {
    // Ensure entries are sorted by luminance (ascending)
    std::sort(entries.begin(), entries.end(),
              [](const PaletteEntry& a, const PaletteEntry& b) {
                  return a.luminance < b.luminance;
              });

    // Infer parameters for clone()/inspection. Assumes the palette is roughly monochrome:
    // entries[i].linearRGB ≈ hue * t_i for some direction hue and scalar t_i.
    glm::vec3 sumDir(0.0f);
    for (const auto& e : entries) {
        glm::vec3 lin = PaletteUtil::sRGB255ToLinear1(e.rgb);
        if (glm::length(lin) > 1e-6f) {
            sumDir += lin;
        }
    }

    glm::vec3 hueLin(1.0f, 1.0f, 1.0f);
    if (glm::length(sumDir) > 1e-6f) {
        hueLin = glm::normalize(sumDir);
    }

    glm::vec3 hue255f = PaletteUtil::Linear1ToSrgb255(hueLin);
    _hueDir = glm::ivec3(
        static_cast<int>(hue255f.r + 0.5f),
        static_cast<int>(hue255f.g + 0.5f),
        static_cast<int>(hue255f.b + 0.5f)
    );

    float minT = std::numeric_limits<float>::infinity();
    float maxT = -std::numeric_limits<float>::infinity();
    for (const auto& e : entries) {
        glm::vec3 lin = PaletteUtil::sRGB255ToLinear1(e.rgb);
        const float t = glm::dot(lin, hueLin);
        minT = std::min(minT, t);
        maxT = std::max(maxT, t);
    }

    if (!std::isfinite(minT) || !std::isfinite(maxT)) {
        minT = 0.0f;
        maxT = 0.0f;
    }
    if (minT > maxT) std::swap(minT, maxT);
    _darkL = minT;
    _lightL = maxT;
}

MonochromePalette::MonochromePalette(std::array<PaletteEntry, 16> customEntries) {
    for (int i = 0; i < 16; ++i) {
        entries[i] = customEntries[i];
    }
    InferParamsFromEntries();
}

MonochromePalette::MonochromePalette(
    const std::vector<std::pair<float, std::shared_ptr<Texture>>>& textures,
    const std::vector<std::pair<float, std::shared_ptr<TextureArray>>>& textureArrays)
    : Palette(textures, textureArrays, false)
{
    InferParamsFromEntries();
}

std::unique_ptr<Palette> MonochromePalette::clone() const {
    return std::make_unique<MonochromePalette>(_darkL, _lightL, _hueDir);
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

unsigned int Palette::GetMinLumIdx() const {
    unsigned int best = 0;
    float minLum = entries[0].luminance;
    for (unsigned int k = 1; k < COLOR_COUNT; ++k) {
        if (entries[k].luminance < minLum) {
            minLum = entries[k].luminance;
            best = k;
        }
    }
    return best;
}

unsigned int Palette::GetMaxLumIdx() const {
    unsigned int best = 0;
    float maxLum = entries[0].luminance;
    for (unsigned int k = 1; k < COLOR_COUNT; ++k) {
        if (entries[k].luminance > maxLum) {
            maxLum = entries[k].luminance;
            best = k;
        }
    }
    return best;
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