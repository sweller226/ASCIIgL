#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>
#include <cstdint>
#include <mutex>

#include <glm/glm.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/TerrainResult.hpp>

class FastNoiseLite;

class TerrainGenerator {
public:
    TerrainGenerator(entt::registry &registry);
    ~TerrainGenerator();
    
    /// Thread-safe: generate terrain for one chunk into \p result (blocks + crossChunkBlocks for trees).
    /// Uses read-only bsr. Tree blocks are appended to result.crossChunkBlocks for main-thread application.
    void GenerateChunkInto(ChunkCoord coord, TerrainResult& result, const blockstate::BlockStateRegistry* bsr);

private:
    entt::registry &m_registry;
    blockstate::BlockStateRegistry* m_bsr;

    struct TerrainParams {
        int BASE_HEIGHT, MIN_TERRAIN_HEIGHT, MAX_TERRAIN_HEIGHT, DIRT_DEPTH;
        float AMPLITUDE;
        int MIN_CAVE_HEIGHT;
        float CAVE_THRESHOLD;
        float VERTICAL_STRETCH;
        float TREE_CHANCE_ADJUSTMENT;
    };

    // Noise generators
    std::unique_ptr<FastNoiseLite> terrainNoise;
    std::unique_ptr<FastNoiseLite> caveNoise1;
    std::unique_ptr<FastNoiseLite> caveNoise2;
    std::unique_ptr<FastNoiseLite> treeNoise;
    std::unique_ptr<FastNoiseLite> forestDensityNoise;

    std::once_flag noiseInitFlag;

    void InitializeNoiseGenerators();
    TerrainParams GetTerrainParams() const;
    
    // Helper functions
    glm::ivec3 LocalToWorldCoord(const ChunkCoord& coord, int localX, int localZ) const;
    uint32_t GetBlockStateAt(int worldX, int worldY, int worldZ, int terrainHeight,
                            const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions,
                            const blockstate::BlockStateRegistry* bsrOverride = nullptr);
    
    // Terrain calculation
    int CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params) const;
    
    bool ShouldCarveCave(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params) const;
    uint32_t DetermineBlockState(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions,
                                 const blockstate::BlockStateRegistry* bsrOverride = nullptr);
    void CheckTreePlacement(int worldX, int worldY, int worldZ, const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions) const;

    /// Same as GenerateTree but appends (worldPos, stateId) to \p out for async application on the main thread.
    void GenerateTreeInto(int worldX, int worldY, int worldZ, const blockstate::BlockStateRegistry* bsr,
                         std::vector<WorldBlockPlacement>& out);
};
