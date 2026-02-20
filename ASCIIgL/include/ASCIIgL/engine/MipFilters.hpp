#pragma once

#include <functional>
#include <cstdint>

namespace ASCIIgL {

namespace MipFilters {

using MipFilterFn = std::function<void(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH)>;

void BoxFilter(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);

void DownSample4x4(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int dstW, int dstH);

} // namespace MipFilters

} 