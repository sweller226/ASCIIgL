#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>

#include <glm/glm.hpp>

#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/Block.hpp>

class FastNoiseLite;

class TerrainGenerator {
public:
    // Callback types for World operations
    using SetBlockQuietCallback = std::function<void(int, int, int, const Block&, std::unordered_set<Chunk*>&)>;
    
    TerrainGenerator();
    ~TerrainGenerator();
    
    void GenerateChunk(Chunk* chunk, 
                      SetBlockQuietCallback setBlockQuiet = nullptr);
    
    // Test/debug generation methods
    void GenerateGrassLayerChunk(Chunk* chunk);
    void GenerateRandomBlockChunk(Chunk* chunk);
    void GenerateOneBlockGrassChunk(Chunk* chunk);

private:
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
    std::unique_ptr<FastNoiseLite> forestDensityNoise;  // For forest regions

    bool noiseInitialized;

    void InitializeNoiseGenerators();
    TerrainParams GetTerrainParams() const;
    
    // Core generation pipeline
    void GenerateTerrainColumn(Chunk* chunk, const ChunkCoord& coord, const TerrainParams& params,
                              std::vector<glm::ivec3>& treePlacementPositions);
    void GenerateTrees(Chunk* chunk, const std::vector<glm::ivec3>& treePlacementPositions,
                      SetBlockQuietCallback setBlockQuiet);
    
    // Helper functions
    glm::ivec3 LocalToWorldPos(const ChunkCoord& coord, int localX, int localZ) const;
    BlockType GetBlockTypeAt(int worldX, int worldY, int worldZ, int terrainHeight,
                            const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions);
    
    // Terrain calculation
    int CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params) const;
    
    bool ShouldCarveCave(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params) const;
    BlockType DetermineBlockType(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions);
    void CheckTreePlacement(int worldX, int worldY, int worldZ, const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions) const;

    void GenerateTree(int worldX, int worldY, int worldZ, SetBlockQuietCallback setBlockQuiet, 
        std::unordered_set<Chunk*>& affectedChunks);
};
