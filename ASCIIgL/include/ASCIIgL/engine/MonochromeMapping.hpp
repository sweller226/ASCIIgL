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
};

// Apply monochrome mapping in-place to an RGBA8 buffer (width*height*4 bytes).
// Uses the same luminance/gradient logic as ColorMonochrome.hlsl.
void ApplyMonochromeMappingRGBA8(uint8_t* rgbaData, int width, int height, const MonochromeMapping& mapping);

} // namespace ASCIIgL

