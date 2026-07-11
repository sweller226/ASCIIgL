#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include "TerrainGenerator_Internal.hpp"

#include <algorithm>
#include <cmath>

#include <FastNoiseLite/FastNoiseLite.h>

#include <ASCIICraft/world/Sizes.hpp>

using namespace terrain_generator_internal;

TerrainGenerator::TerrainParams TerrainGenerator::GetTerrainParams() const {
    TerrainParams params;
    params.BASE_HEIGHT = 5;
    params.MIN_TERRAIN_HEIGHT = 3;
    params.MAX_TERRAIN_HEIGHT = 28;
    params.AMPLITUDE = 1.65f;
    params.DIRT_DEPTH = 3;
    params.SEA_LEVEL = 75;
    return params;
}

BiomeType TerrainGenerator::GetBiomeType(int worldx, int worldz) {
    InitializeNoiseGenerators();
    const float t = SampleBiomeTemperature(worldx, worldz);
    const float h = SampleBiomeHumidity(worldx, worldz);
    return DominantBiome(ComputeBiomeWeights(t, h));
}

float TerrainGenerator::EstimateTerrainSlope(int worldX, int worldZ, const TerrainParams& params) const {
    const int center = CalculateTerrainHeight(worldX, worldZ, params, BiomeType::Forest);
    const int east = CalculateTerrainHeight(worldX + 1, worldZ, params, BiomeType::Forest);
    const int west = CalculateTerrainHeight(worldX - 1, worldZ, params, BiomeType::Forest);
    const int north = CalculateTerrainHeight(worldX, worldZ + 1, params, BiomeType::Forest);
    const int south = CalculateTerrainHeight(worldX, worldZ - 1, params, BiomeType::Forest);
    return static_cast<float>(std::max({
        std::abs(east - center),
        std::abs(west - center),
        std::abs(north - center),
        std::abs(south - center)
    }));
}

TerrainGenerator::TerrainColumnSample TerrainGenerator::SampleTerrainColumn(
    int worldX, int worldZ, const TerrainParams& params) const {

    const float t = SampleBiomeTemperature(worldX, worldZ);
    const float h = SampleBiomeHumidity(worldX, worldZ);
    const BiomeWeights weights = ComputeBiomeWeights(t, h);

    TerrainColumnSample sample;
    sample.dominantBiome = DominantBiome(weights);
    sample.terrainHeight = CalculateTerrainHeight(worldX, worldZ, params, sample.dominantBiome);
    sample.flowerForestWeight = weights.flowerForest;
    sample.forestWeight = weights.forest;
    sample.valleyFactor = SampleValleyFactor(worldX, worldZ);
    sample.slope = EstimateTerrainSlope(worldX, worldZ, params);
    return sample;
}

float TerrainGenerator::CalculateTerrainHeightChunksForBiome(
    int worldX, int worldZ, const TerrainParams& params, BiomeType biomeType) const {

    const float terrainBaseNoise = terrainNoise->GetNoise((float)worldX, (float)worldZ);
    const float t = SampleBiomeTemperature(worldX, worldZ);
    const float h = SampleBiomeHumidity(worldX, worldZ);

    float amplitudeScale = 1.0f;
    float baseHeightOffset = 0.0f;
    float minChunks = static_cast<float>(params.MIN_TERRAIN_HEIGHT);
    float maxChunks = static_cast<float>(params.MAX_TERRAIN_HEIGHT);

    switch (biomeType) {
        case BiomeType::FlowerForest:
            amplitudeScale = 0.60f;
            baseHeightOffset = -0.04f;
            minChunks = 4.6f;
            maxChunks = 7.4f;
            break;
        case BiomeType::Forest:
            amplitudeScale = 0.94f;
            baseHeightOffset = 0.14f;
            minChunks = 4.4f;
            maxChunks = 8.6f;
            break;
        default:
            amplitudeScale = 0.94f;
            baseHeightOffset = 0.14f;
            minChunks = 4.4f;
            maxChunks = 8.6f;
            break;
    }

    const float climateWarp = ((t - 0.5f) * 0.10f) + ((h - 0.5f) * 0.10f);
    const float biomeAmplitude = params.AMPLITUDE * amplitudeScale;
    float terrainHeightChunks = (params.BASE_HEIGHT + baseHeightOffset) + ((terrainBaseNoise + climateWarp) * biomeAmplitude);

    if (biomeType == BiomeType::FlowerForest && terrainBaseNoise > 0.68f) {
        terrainHeightChunks += (terrainBaseNoise - 0.68f) * 1.8f;
    } else if (biomeType == BiomeType::Forest && terrainBaseNoise > 0.56f) {
        terrainHeightChunks += (terrainBaseNoise - 0.56f) * 3.4f;
    }

    terrainHeightChunks = std::max(minChunks, std::min(maxChunks, terrainHeightChunks));
    return terrainHeightChunks;
}

int TerrainGenerator::CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params, BiomeType /*biomeType*/) const {
    const float t = SampleBiomeTemperature(worldX, worldZ);
    const float h = SampleBiomeHumidity(worldX, worldZ);
    const BiomeWeights weights = ComputeBiomeWeights(t, h);

    float terrainHeightChunks =
        (weights.flowerForest * CalculateTerrainHeightChunksForBiome(worldX, worldZ, params, BiomeType::FlowerForest)) +
        (weights.forest * CalculateTerrainHeightChunksForBiome(worldX, worldZ, params, BiomeType::Forest));

    terrainHeightChunks = std::max(
        static_cast<float>(params.MIN_TERRAIN_HEIGHT),
        std::min(static_cast<float>(params.MAX_TERRAIN_HEIGHT), terrainHeightChunks));

    return static_cast<int>(terrainHeightChunks * sizes::CHUNK_SIZE);
}
