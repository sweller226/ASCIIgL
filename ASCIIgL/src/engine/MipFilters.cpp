#include <ASCIIgL/engine/MipFilters.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace ASCIIgL {

namespace MipFilters {

static inline const uint8_t* SampleRGBAClamp(const uint8_t* srcData, int srcW, int srcH, int x, int y) {
    x = std::clamp(x, 0, srcW - 1);
    y = std::clamp(y, 0, srcH - 1);
    return &srcData[(static_cast<size_t>(y) * static_cast<size_t>(srcW) + static_cast<size_t>(x)) * 4];
}

static inline bool RGBAEqual(const uint8_t* a, const uint8_t* b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

void BoxFilter(
    const uint8_t* srcData, int srcW, int srcH,
    uint8_t* dstData, int dstW, int dstH)
{
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {

            int sx = x * 2;
            int sy = y * 2;

            const uint8_t* p00 = SampleRGBAClamp(srcData, srcW, srcH, sx,     sy);
            const uint8_t* p10 = SampleRGBAClamp(srcData, srcW, srcH, sx + 1, sy);
            const uint8_t* p01 = SampleRGBAClamp(srcData, srcW, srcH, sx,     sy + 1);
            const uint8_t* p11 = SampleRGBAClamp(srcData, srcW, srcH, sx + 1, sy + 1);

            size_t di = (y * dstW + x) * 4;

            for (int c = 0; c < 4; ++c) {
                dstData[di + c] = (p00[c] + p10[c] + p01[c] + p11[c]) / 4;
            }
        }
    }
}

void PixelArtMode2x2(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH) {
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            const int sx = x * 2;
            const int sy = y * 2;

            const uint8_t* p[4] = {
                SampleRGBAClamp(srcData, srcW, srcH, sx,     sy),
                SampleRGBAClamp(srcData, srcW, srcH, sx + 1, sy),
                SampleRGBAClamp(srcData, srcW, srcH, sx,     sy + 1),
                SampleRGBAClamp(srcData, srcW, srcH, sx + 1, sy + 1),
            };

            // Find most frequent exact RGBA among the 4 samples (mode).
            // Tie-break is stable: earlier sample wins (p00 > p10 > p01 > p11).
            int bestIdx = 0;
            int bestCount = 1;
            for (int i = 0; i < 4; ++i) {
                int count = 1;
                for (int j = i + 1; j < 4; ++j) {
                    if (RGBAEqual(p[i], p[j])) count++;
                }
                if (count > bestCount) {
                    bestCount = count;
                    bestIdx = i;
                }
            }

            const uint8_t* out = p[bestIdx];
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x)) * 4;
            dstData[di + 0] = out[0];
            dstData[di + 1] = out[1];
            dstData[di + 2] = out[2];
            dstData[di + 3] = out[3];
        }
    }
}

void PixelArtAlphaCutoutAny2x2(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH) {
    constexpr uint8_t kCutoff = 128; // matches common alpha-test cutoff of 0.5

    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            const int sx = x * 2;
            const int sy = y * 2;

            const uint8_t* s[4] = {
                SampleRGBAClamp(srcData, srcW, srcH, sx,     sy),
                SampleRGBAClamp(srcData, srcW, srcH, sx + 1, sy),
                SampleRGBAClamp(srcData, srcW, srcH, sx,     sy + 1),
                SampleRGBAClamp(srcData, srcW, srcH, sx + 1, sy + 1),
            };

            int bestIdx = 0;
            uint8_t bestA = s[0][3];
            bool anyOpaque = (bestA >= kCutoff);
            for (int i = 1; i < 4; ++i) {
                const uint8_t a = s[i][3];
                if (a > bestA) {
                    bestA = a;
                    bestIdx = i;
                }
                if (a >= kCutoff) anyOpaque = true;
            }

            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x)) * 4;
            if (!anyOpaque) {
                dstData[di + 0] = 0;
                dstData[di + 1] = 0;
                dstData[di + 2] = 0;
                dstData[di + 3] = 0;
                continue;
            }

            // Keep binary alpha in mips so alpha-test doesn't \"fade out\" at distance.
            const uint8_t* p = s[bestIdx];
            dstData[di + 0] = p[0];
            dstData[di + 1] = p[1];
            dstData[di + 2] = p[2];
            dstData[di + 3] = 255;
        }
    }
}

void DownSample4x4(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int dstW, int dstH) {
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            const int sx0 = x * 4;
            const int sy0 = y * 4;

            int sum[4] = { 0, 0, 0, 0 };
            for (int oy = 0; oy < 4; ++oy) {
                for (int ox = 0; ox < 4; ++ox) {
                    const uint8_t* p = SampleRGBAClamp(src, srcW, srcH, sx0 + ox, sy0 + oy);
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                    sum[3] += p[3];
                }
            }

            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x)) * 4;
            dst[di + 0] = static_cast<uint8_t>(sum[0] / 16);
            dst[di + 1] = static_cast<uint8_t>(sum[1] / 16);
            dst[di + 2] = static_cast<uint8_t>(sum[2] / 16);
            dst[di + 3] = static_cast<uint8_t>(sum[3] / 16);
        }
    }
}

} // namespace MipFilters

} // namespace ASCIIgL