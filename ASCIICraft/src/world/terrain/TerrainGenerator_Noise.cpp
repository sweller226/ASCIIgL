#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include "TerrainGenerator_Internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>

#include <FastNoiseLite.h>

using namespace terrain_generator_internal;

namespace {

/// Deterministic per-layer seed from one master world seed (\p salt distinguishes noise streams).
inline int DerivedNoiseSeed(uint64_t worldSeed, uint64_t salt) {
    uint64_t x = worldSeed + 0x9e3779b97f4a7c15ULL + (salt << 6) + (salt >> 2);
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return static_cast<int>(x & 0x7fffffff);
}

constexpr uint64_t kSaltTerrain = 1;
constexpr uint64_t kSaltForestDensity = 2;
constexpr uint64_t kSaltTree = 3;
constexpr uint64_t kSaltFlowerDensity = 4;
constexpr uint64_t kSaltBiomeTemperature = 5;
constexpr uint64_t kSaltBiomeHumidity = 6;
constexpr uint64_t kSaltSurfaceDepth = 7;
constexpr uint64_t kSaltTerrainWarp = 10;
constexpr uint64_t kSaltValley = 11;

} // namespace

void TerrainGenerator::InitializeNoiseGenerators() {
    std::call_once(noiseInitFlag, [this]() {
    terrainNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    terrainNoise->SetFrequency(0.018f);
    terrainNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise->SetFractalOctaves(4);
    terrainNoise->SetFractalLacunarity(2.0f);
    terrainNoise->SetFractalGain(0.5f);
    terrainNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltTerrain));

    // Forest density noise
    forestDensityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    forestDensityNoise->SetFrequency(0.01f);
    forestDensityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    forestDensityNoise->SetFractalOctaves(4);
    forestDensityNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltForestDensity));

    // Flower density noise
    flowerDensityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    flowerDensityNoise->SetFrequency(0.01f);
    flowerDensityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    flowerDensityNoise->SetFractalOctaves(3);
    flowerDensityNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltFlowerDensity));

    // Biome climate noises (large, smooth regions)
    biomeTemperatureNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    biomeTemperatureNoise->SetFrequency(0.0025f);
    biomeTemperatureNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeTemperatureNoise->SetFractalOctaves(3);
    biomeTemperatureNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltBiomeTemperature));

    biomeHumidityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    biomeHumidityNoise->SetFrequency(0.0022f);
    biomeHumidityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeHumidityNoise->SetFractalOctaves(3);
    biomeHumidityNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltBiomeHumidity));

    // Surface depth noise used for dirt thickness variation.
    surfaceDepthNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    surfaceDepthNoise->SetFrequency(0.015f);
    surfaceDepthNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    surfaceDepthNoise->SetFractalOctaves(2);
    surfaceDepthNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltSurfaceDepth));

    // Domain warp and valleys keep broad terrain variation without a mountain biome.
    terrainWarpNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    terrainWarpNoise->SetFrequency(0.0038f);
    terrainWarpNoise->SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive);
    terrainWarpNoise->SetFractalOctaves(3);
    terrainWarpNoise->SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    terrainWarpNoise->SetDomainWarpAmp(42.0f);
    terrainWarpNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltTerrainWarp));

    valleyNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    valleyNoise->SetFrequency(0.006f);
    valleyNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    valleyNoise->SetFractalOctaves(4);
    valleyNoise->SetFractalLacunarity(2.1f);
    valleyNoise->SetFractalGain(0.45f);
    valleyNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltValley));

    treeNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    treeNoise->SetFrequency(0.25f);
    treeNoise->SetFractalType(FastNoiseLite::FractalType_None);
    treeNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltTree));
    });
}

float TerrainGenerator::SampleBiomeTemperature(int worldX, int worldZ) const {
    return (biomeTemperatureNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
}

float TerrainGenerator::SampleBiomeHumidity(int worldX, int worldZ) const {
    return (biomeHumidityNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
}

float TerrainGenerator::SampleValleyFactor(int worldX, int worldZ) const {
    float wx = static_cast<float>(worldX);
    float wz = static_cast<float>(worldZ);
    terrainWarpNoise->DomainWarp(wx, wz);
    const float valleyNoiseValue = valleyNoise->GetNoise(wx, wz);
    const float channel = 1.0f - std::abs(valleyNoiseValue);
    return std::clamp(SmoothStep(0.70f, 0.96f, channel), 0.0f, 1.0f);
}
