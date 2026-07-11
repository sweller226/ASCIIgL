#pragma once

#include <algorithm>
#include <cstdint>

#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

namespace terrain_generator_internal {

inline float SmoothStep(float edge0, float edge1, float x) {
    if (edge0 >= edge1) {
        return x >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct BiomeWeights {
    float flowerForest;
    float forest;
};

inline BiomeWeights ComputeBiomeWeights(float temperature, float humidity) {
    const float flowerHumidW = SmoothStep(0.54f, 0.68f, humidity);
    const float flowerTempW = SmoothStep(0.28f, 0.42f, temperature) * (1.0f - SmoothStep(0.70f, 0.76f, temperature));
    const float flowerW = flowerHumidW * flowerTempW;
    const float forestW = std::max(0.0f, 1.0f - flowerW);
    const float sum = flowerW + forestW;
    if (sum < 1e-6f) {
        return {0.0f, 1.0f};
    }
    return {flowerW / sum, forestW / sum};
}

inline BiomeWeights NormalizeBiomeWeights(BiomeWeights weights) {
    const float sum = weights.flowerForest + weights.forest;
    if (sum < 1e-6f) {
        return {0.0f, 1.0f};
    }
    return {
        weights.flowerForest / sum,
        weights.forest / sum
    };
}

inline BiomeType DominantBiome(const BiomeWeights& weights) {
    if (weights.flowerForest >= weights.forest) {
        return BiomeType::FlowerForest;
    }
    return BiomeType::Forest;
}

inline float BiomeWeightFor(BiomeType biome, const BiomeWeights& weights) {
    switch (biome) {
        case BiomeType::FlowerForest: return weights.flowerForest;
        case BiomeType::Forest: return weights.forest;
        default: return weights.forest;
    }
}

inline float RandomFloat(int x, int z) {
    uint32_t seed = x * 374761393u + z * 668265263u;
    seed = (seed ^ (seed >> 13)) * 1274126177u;
    seed ^= (seed >> 16);
    return (seed & 0xFFFFFF) / float(0xFFFFFF);
}

inline int RandomRange(int x, int z, int lo, int hi) {
    if (lo >= hi) return lo;
    const int span = hi - lo + 1;
    return lo + std::min(span - 1, static_cast<int>(RandomFloat(x, z) * span));
}

} // namespace terrain_generator_internal
