#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>
#include <cstdint>

#include <glm/glm.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/world/Chunk.hpp>

class FastNoiseLite;

class TerrainGenerator {
public:
    // Callback types for World operations (uses blockstate IDs)
    using SetBlockCallback = std::function<void(int, int, int, uint32_t)>;
    
    TerrainGenerator(entt::registry &registry);
    ~TerrainGenerator();
    
    void GenerateChunk(Chunk* chunk, 
                      SetBlockCallback setBlockQuiet = nullptr);

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

    bool noiseInitialized;

    void InitializeNoiseGenerators();
    TerrainParams GetTerrainParams() const;
    
    // Core generation pipeline
    void GenerateTerrainColumns(Chunk* chunk, const ChunkCoord& coord, const TerrainParams& params,
                              std::vector<glm::ivec3>& treePlacementPositions);
    void GenerateTrees(Chunk* chunk, const std::vector<glm::ivec3>& treePlacementPositions,
                      SetBlockCallback setBlockQuiet);
    
    // Helper functions
    glm::ivec3 LocalToWorldCoord(const ChunkCoord& coord, int localX, int localZ) const;
    uint32_t GetBlockStateAt(int worldX, int worldY, int worldZ, int terrainHeight,
                            const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions);
    
    // Terrain calculation
    int CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params) const;
    
    bool ShouldCarveCave(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params) const;
    uint32_t DetermineBlockState(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions);
    void CheckTreePlacement(int worldX, int worldY, int worldZ, const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions) const;

    void GenerateTree(int worldX, int worldY, int worldZ, SetBlockCallback setBlockQuiet);
};
