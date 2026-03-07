#pragma once

#include <ASCIIgL/engine/MipFilters.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ASCIIgL {

namespace MipChain {

struct RGBA8Level {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
};

inline int ComputeMaxPossibleLevels(int w, int h) {
    int levels = 1;
    while (w > 1 || h > 1) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        levels++;
    }
    return levels;
}

inline int ClampTargetLevels(int baseW, int baseH, int maxLevels) {
    const int maxPossible = ComputeMaxPossibleLevels(baseW, baseH);
    if (maxLevels < 0) return maxPossible;
    return std::min(maxLevels, maxPossible);
}

// Build a full mip chain from a base RGBA8 level. The base data is moved into mip 0.
inline std::vector<RGBA8Level> BuildRGBA8(std::vector<uint8_t>&& baseData, int baseW, int baseH,
                                          int maxLevels, MipFilters::MipFilterFn filter)
{
    std::vector<RGBA8Level> chain;
    if (baseW <= 0 || baseH <= 0) return chain;

    if (!filter) filter = MipFilters::BoxFilter;

    const int targetLevels = ClampTargetLevels(baseW, baseH, maxLevels);
    chain.reserve(targetLevels);

    RGBA8Level mip0;
    mip0.width = baseW;
    mip0.height = baseH;
    mip0.data = std::move(baseData);
    chain.push_back(std::move(mip0));

    for (int level = 1; level < targetLevels; ++level) {
        const RGBA8Level& prev = chain[level - 1];

        const int newW = std::max(1, prev.width / 2);
        const int newH = std::max(1, prev.height / 2);

        RGBA8Level next;
        next.width = newW;
        next.height = newH;
        next.data.resize(static_cast<size_t>(newW) * static_cast<size_t>(newH) * 4);

        filter(prev.data.data(), prev.width, prev.height,
               next.data.data(), newW, newH);

        chain.push_back(std::move(next));
    }

    return chain;
}

} // namespace MipChain

} // namespace ASCIIgL

