#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include "TerrainGenerator_Internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <FastNoiseLite.h>

#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>

using namespace terrain_generator_internal;

uint32_t TerrainGenerator::GetBlockStateAt(int worldX, int worldY, int worldZ, const TerrainColumnSample& sample,
                                          const TerrainParams& params,
                                          std::vector<glm::ivec3>& treePlacementPositions,
                                          std::vector<glm::ivec3>& flowerPlacementPositions,
                                          const blockstate::BlockStateRegistry* bsrOverride, BiomeType biomeType) {
    const blockstate::BlockStateRegistry* bsr = bsrOverride ? bsrOverride : m_bsr;
    if (!bsr) return 0u;
    uint32_t dirtId = bsr->GetDefaultState("minecraft:dirt");
    uint32_t airId = bsr->GetDefaultState("minecraft:air");

    // Solid dirt layer at world bottom
    if (worldY == 0) {
        return dirtId;
    }

    // Air above terrain
    if (worldY > sample.terrainHeight) {
        return airId;
    }

    const int depthFromSurface = sample.terrainHeight - worldY;
    return DetermineBlockState(
        worldX, worldY, worldZ, depthFromSurface, params, treePlacementPositions, flowerPlacementPositions,
        bsrOverride, biomeType, &sample
    );
}

uint32_t TerrainGenerator::DetermineBlockState(int worldX, int worldY, int worldZ, int depthFromSurface,
                                              const TerrainParams& params,
                                              std::vector<glm::ivec3>& treePlacementPositions,
                                              std::vector<glm::ivec3>& flowerPlacementPositions,
                                              const blockstate::BlockStateRegistry* bsrOverride, BiomeType biomeType,
                                              const TerrainColumnSample* sample) {
    const blockstate::BlockStateRegistry* bsr = bsrOverride ? bsrOverride : m_bsr;
    if (!bsr) return 0u;
    uint32_t grassId = bsr->GetDefaultState("minecraft:grass");
    uint32_t dirtId = bsr->GetDefaultState("minecraft:dirt");
    const int baseDirtDepth = params.DIRT_DEPTH;
    const BiomeWeights weights = ComputeBiomeWeights(
        SampleBiomeTemperature(worldX, worldZ),
        SampleBiomeHumidity(worldX, worldZ));
    const float valleyFactor = sample ? sample->valleyFactor : 0.0f;
    const float slope = sample ? sample->slope : 0.0f;
    const int terrainHeight = sample ? sample->terrainHeight : worldY + depthFromSurface;
    const int elevationAboveSea = terrainHeight - params.SEA_LEVEL;
    const float biomeDepthOffset =
        (weights.flowerForest * 0.0f) +
        (weights.forest * 1.0f);
    const float depthNoise = (surfaceDepthNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
    const int depthFromNoise = (depthNoise < 0.25f) ? -1 : ((depthNoise > 0.75f) ? 1 : 0);
    const int slopeDepthPenalty = (slope > 7.0f) ? 2 : ((slope > 4.0f) ? 1 : 0);
    const int valleyDepthBonus = (valleyFactor > 0.58f && slope < 3.0f) ? 1 : 0;
    const int biomeDirtDepth = std::max(
        1,
        baseDirtDepth + static_cast<int>(std::round(biomeDepthOffset)) +
        depthFromNoise - slopeDepthPenalty + valleyDepthBonus
    );
    const bool highAltitude = elevationAboveSea > 52;
    const bool sheerSlope = slope > 7.0f;
    const bool rockySurface = highAltitude && sheerSlope;

    if (depthFromSurface == 0) {
        if (worldY < params.SEA_LEVEL) {
            if (rockySurface || slope > 5.0f) {
                return dirtId;
            }
            return dirtId;
        }

        if (rockySurface) {
            return dirtId;
        }

        if (sample) {
            CheckTreePlacement(worldX, worldY, worldZ, params, biomeType, *sample, treePlacementPositions);
        }

        const float flowerNoise = flowerDensityNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
        const float flowerChance = (flowerNoise + 1.0f) * 0.5f;
        if (slope < 4.0f && weights.flowerForest > 0.22f) {
            const float fineFlower = (flowerDensityNoise->GetNoise(
                static_cast<float>(worldX) * 0.13f, static_cast<float>(worldZ) * 0.13f) + 1.0f) * 0.5f;
            const float spreadRoll = RandomFloat(worldX + 19, worldZ - 23);
            const float targetChance = 0.58f * weights.flowerForest;
            const float dither = (fineFlower - 0.5f) * 0.22f;
            if (spreadRoll < std::clamp(targetChance + dither, 0.0f, 1.0f)) {
                flowerPlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
            }
        } else if (slope < 5.0f && weights.forest > 0.25f && flowerChance > 0.92f &&
                   RandomFloat(worldX + 3, worldZ - 5) < 0.02f * weights.forest) {
            flowerPlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        }

        return blockplacement::FinalizePlacedStateBasic(
            *bsr, grassId, worldX, worldY, worldZ, blockplacement::PlacementContext::TerrainGeneration
        );
    } else if (rockySurface && depthFromSurface < 3) {
        return dirtId;
    } else if (depthFromSurface < biomeDirtDepth) {
        return dirtId;
    } else {
        return dirtId;
    }
}

void TerrainGenerator::CheckTreePlacement(
    int worldX, int worldY, int worldZ,
    const TerrainParams& params,
    BiomeType biomeType,
    const TerrainColumnSample& sample,
    std::vector<glm::ivec3>& treePlacementPositions) const
{
    if (worldY < params.SEA_LEVEL)
        return;
    if (sample.slope > 8.0f)
        return;

    const BiomeWeights weights{
        sample.flowerForestWeight,
        sample.forestWeight
    };

    float forestDensity = forestDensityNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    forestDensity = (forestDensity + 1.0f) * 0.5f;

    const float biomeTreeDensityMul =
        (weights.flowerForest * 0.78f) +
        (weights.forest * 1.15f);
    forestDensity = std::clamp(forestDensity * biomeTreeDensityMul, 0.0f, 1.0f);

    if (forestDensity < 0.04f) {
        return;
    }

    const bool denseGrove = forestDensity > 0.56f && weights.forest > 0.55f;
    const int minTreeSpacing = denseGrove ? 4 : (weights.flowerForest > weights.forest ? 6 : 5);

    for (const auto& existing : treePlacementPositions) {
        if (std::abs(existing.x - worldX) < minTreeSpacing &&
            std::abs(existing.z - worldZ) < minTreeSpacing) {
            return;
        }
    }

    const float minDensity = 0.05f * (biomeTreeDensityMul / 0.88f);
    if (forestDensity < minDensity) {
        return;
    }

    float noiseHash = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));

    float north = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ + 1));
    float south = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ - 1));
    float east = treeNoise->GetNoise(static_cast<float>(worldX + 1), static_cast<float>(worldZ));
    float west = treeNoise->GetNoise(static_cast<float>(worldX - 1), static_cast<float>(worldZ));

    bool isLocalMax = (noiseHash > north) && (noiseHash > south) &&
                      (noiseHash > east) && (noiseHash > west);

    if (isLocalMax) {
        float normalizedHash = (noiseHash + 1.0f) * 0.5f;
        const float groveBonus = denseGrove ? 0.09f : 0.0f;
        const float densityFactor = 0.36f + groveBonus - weights.flowerForest * 0.08f;
        float threshold = 1.0f - (forestDensity * densityFactor);

        if (normalizedHash > threshold) {
            treePlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        }
    }
}
