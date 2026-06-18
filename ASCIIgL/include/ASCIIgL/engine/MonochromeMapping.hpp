#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace ASCIIgL {

// Configuration for baking textures to a monochrome gradient on load.
struct MonochromeMapping {
    bool enabled = false;
    float darkL = 0.0f;            // linear luminance at dark end
    float lightL = 1.0f;           // linear luminance at bright end
    glm::ivec3 hueDir = glm::ivec3(255); // hue direction in sRGB 0–255

    // Post-process controls applied to sampled luminance before mapping.
    // brightness: multiplicative scale in normalized luminance space (0 = black, 1 = unchanged, >1 brighter).
    // contrast: multiplies around 0.5 (0 = flat 0.5, 1 = unchanged, >1 increases contrast).
    float brightness = 1.0f;
    float contrast = 1.0f;
};

// Apply monochrome mapping in-place to an RGBA8 buffer (width*height*4 bytes).
// Uses the same luminance/gradient logic as ColorUtil.hlsl.
void ApplyMonochromeMappingRGBA8(uint8_t* rgbaData, int width, int height, const MonochromeMapping& mapping);

} // namespace ASCIIgL

