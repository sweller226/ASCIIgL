#pragma once

#include <functional>
#include <cstdint>

namespace ASCIIgL {

namespace MipFilters {

using MipFilterFn = std::function<void(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH)>;

void BoxFilter(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);

// Pixel-art tuned downsampling (RGBA8):
// - Mode2x2: picks the most frequent exact RGBA color in the 2x2 block (crisp, preserves flats).
// - AlphaCutoutAny2x2: for alpha-test textures; if ANY source alpha >= 128 then output alpha=255 and RGB from max-alpha sample, else alpha=0.
void PixelArtMode2x2(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);
void PixelArtAlphaCutoutAny2x2(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);

void DownSample4x4(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int dstW, int dstH);

} // namespace MipFilters

} 