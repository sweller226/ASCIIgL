#pragma once

#include <functional>
#include <cstdint>

namespace ASCIIgL {

namespace MipFilters {

using MipFilterFn = std::function<void(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH)>;

void BoxFilter(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);

// Pixel-art tuned downsampling (RGBA8):
// - Mode2x2: picks the most frequent exact RGBA color in the 2x2 block (crisp, preserves flats).
// - AlphaCutoutCoverage2x2: for alpha-test textures. RGB from an opaque sample (no transparent
//   black bleed); alpha = coverage (opaqueCount/4) so clip(a-0.5) thins cutouts with distance
//   instead of inflating them.
void PixelArtMode2x2(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);
void PixelArtAlphaCutoutCoverage2x2(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);

void DownSample4x4(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int dstW, int dstH);

} // namespace MipFilters

} 