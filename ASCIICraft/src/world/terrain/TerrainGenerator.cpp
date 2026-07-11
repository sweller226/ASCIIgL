#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include <memory>

#include <FastNoiseLite.h>

#include <ASCIIgL/util/Logger.hpp>

TerrainGenerator::TerrainGenerator(entt::registry& registry, uint64_t worldSeed)
    : m_registry(registry)
    , m_worldSeed(worldSeed) {
    terrainNoise = std::make_unique<FastNoiseLite>();
    treeNoise = std::make_unique<FastNoiseLite>();
    forestDensityNoise = std::make_unique<FastNoiseLite>();
    flowerDensityNoise = std::make_unique<FastNoiseLite>();
    biomeTemperatureNoise = std::make_unique<FastNoiseLite>();
    biomeHumidityNoise = std::make_unique<FastNoiseLite>();
    surfaceDepthNoise = std::make_unique<FastNoiseLite>();
    terrainWarpNoise = std::make_unique<FastNoiseLite>();
    valleyNoise = std::make_unique<FastNoiseLite>();

    if (registry.ctx().contains<blockstate::BlockStateRegistry>()) {
        m_bsr = &registry.ctx().get<blockstate::BlockStateRegistry>();
    } else {
        ASCIIgL::Logger::Warning("BlockStateRegistry missing from context");
        m_bsr = nullptr;
    }
}

TerrainGenerator::~TerrainGenerator() = default;
