#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/terrain/TerrainResult.hpp>

class FastNoiseLite;

enum class BiomeType {
    Plains,
    FlowerForest,
    Forest
};

class TerrainGenerator {
public:
    TerrainGenerator(entt::registry& registry, uint64_t worldSeed);
    ~TerrainGenerator();

    /// Thread-safe: generate terrain into \p blocks (chunk's block buffer, size Chunk::VOLUME) and
    /// append cross-chunk placements (e.g. trees) to \p result.crossChunkBlocks. Call from terrain job only.
    void GenerateChunkInto(ChunkCoord coord, uint32_t* blocks, TerrainResult& result, const blockstate::BlockStateRegistry* bsr);

private:
    entt::registry& m_registry;
    blockstate::BlockStateRegistry* m_bsr;
    uint64_t m_worldSeed;

    struct TerrainParams {
        int BASE_HEIGHT;
        int MIN_TERRAIN_HEIGHT;
        int MAX_TERRAIN_HEIGHT;
        int DIRT_DEPTH;
        float AMPLITUDE;
        int SEA_LEVEL;
    };

    std::unique_ptr<FastNoiseLite> terrainNoise;
    std::unique_ptr<FastNoiseLite> treeNoise;
    std::unique_ptr<FastNoiseLite> forestDensityNoise;
    std::unique_ptr<FastNoiseLite> flowerDensityNoise;
    std::unique_ptr<FastNoiseLite> biomeTemperatureNoise;
    std::unique_ptr<FastNoiseLite> biomeHumidityNoise;
    std::unique_ptr<FastNoiseLite> surfaceDepthNoise;

    std::once_flag noiseInitFlag;

    void InitializeNoiseGenerators();
    TerrainParams GetTerrainParams() const;

    BiomeType GetBiomeType(int worldX, int worldZ);
    float SampleBiomeTemperature(int worldX, int worldZ) const;
    float SampleBiomeHumidity(int worldX, int worldZ) const;
    float CalculateTerrainHeightChunksForBiome(int worldX, int worldZ, const TerrainParams& params, BiomeType biomeType) const;

    glm::ivec3 LocalToWorldCoord(const ChunkCoord& coord, int localX, int localZ) const;
    uint32_t GetBlockStateAt(int worldX, int worldY, int worldZ, int terrainHeight,
                            const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions,
                            std::vector<glm::ivec3>& flowerPlacementPositions,
                            const blockstate::BlockStateRegistry* bsrOverride = nullptr,
                            BiomeType biomeType = BiomeType::Forest);

    int CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params, BiomeType biomeType) const;
    uint32_t DetermineBlockState(int worldX, int worldY, int worldZ, int depthFromSurface,
                                 const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions,
                                 std::vector<glm::ivec3>& flowerPlacementPositions,
                                 const blockstate::BlockStateRegistry* bsrOverride = nullptr,
                                 BiomeType biomeType = BiomeType::Forest);
    void CheckTreePlacement(int worldX, int worldY, int worldZ, const TerrainParams& params, BiomeType biomeType,
                            std::vector<glm::ivec3>& treePlacementPositions) const;

    void GenerateTreeInto(int worldX, int worldY, int worldZ, const blockstate::BlockStateRegistry* bsr,
                         std::vector<WorldBlockPlacement>& out);
};
