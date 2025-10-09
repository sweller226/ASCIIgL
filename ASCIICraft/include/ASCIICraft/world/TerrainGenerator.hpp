#pragma once

#include <vector>
#include <tuple>
#include <memory>
#include <functional>
#include <unordered_set>

#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/Block.hpp>

class FastNoiseLite;

class TerrainGenerator {
public:
    // Callback types for World operations
    using SetBlockQuietCallback = std::function<void(int, int, int, const Block&, std::unordered_set<ChunkCoord>&)>;
    using BatchInvalidateCallback = std::function<void(const ChunkCoord&)>;
    
    TerrainGenerator();
    ~TerrainGenerator();
    
    void GenerateChunk(Chunk* chunk, 
                      SetBlockQuietCallback setBlockQuiet = nullptr,
                      BatchInvalidateCallback batchInvalidate = nullptr);
    
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
        float TREE_CHANCE_THRESHOLD;
    };

    // Noise generators
    std::unique_ptr<FastNoiseLite> terrainNoise;
    std::unique_ptr<FastNoiseLite> caveNoise1;
    std::unique_ptr<FastNoiseLite> caveNoise2;
    std::unique_ptr<FastNoiseLite> treeNoise;

    bool noiseInitialized;

    void InitializeNoiseGenerators();
    TerrainParams GetTerrainParams() const;
    int CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params) const;
    
    void GenerateTerrainColumn(Chunk* chunk, int x, int z, int worldX, int worldZ, int chunkBaseY, int terrainHeight, const TerrainParams& params, std::vector<std::tuple<int, int, int>>& treePlacementPositions);
    
    bool ShouldCarveCave(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params) const;
    BlockType DetermineBlockType(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params, std::vector<std::tuple<int, int, int>>& treePlacementPositions);
    void CheckTreePlacement(int worldX, int worldY, int worldZ, const TerrainParams& params, std::vector<std::tuple<int, int, int>>& treePlacementPositions) const;

    void GenerateTree(int worldX, int worldY, int worldZ, 
                     SetBlockQuietCallback setBlockQuiet,
                     BatchInvalidateCallback batchInvalidate);
};
