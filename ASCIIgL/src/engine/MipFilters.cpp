#include <ASCIIgL/engine/MipFilters.hpp>

#include <algorithm>
#include <cmath>

namespace ASCIIgL {

namespace MipFilters {

void BoxFilter(
    const uint8_t* srcData, int srcW, int srcH,
    uint8_t* dstData, int dstW, int dstH)
{
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {

            int sx = x * 2;
            int sy = y * 2;

            auto sample = [&](int px, int py) {
                px = std::min(srcW - 1, px);
                py = std::min(srcH - 1, py);
                size_t idx = (py * srcW + px) * 4;
                return &srcData[idx];
            };

            const uint8_t* p00 = sample(sx,     sy);
            const uint8_t* p10 = sample(sx + 1, sy);
            const uint8_t* p01 = sample(sx,     sy + 1);
            const uint8_t* p11 = sample(sx + 1, sy + 1);

            size_t di = (y * dstW + x) * 4;

            for (int c = 0; c < 4; ++c) {
                dstData[di + c] =
                    (p00[c] + p10[c] + p01[c] + p11[c]) / 4;
            }
        }
    }
}

} // namespace MipFilters

} // namespace ASCIIgL