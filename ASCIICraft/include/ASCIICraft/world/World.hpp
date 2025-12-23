#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/TerrainGenerator.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertexShaderCPU.hpp>

// Forward declarations
class Player;

// Main World class
class World {
public:
    World(unsigned int renderDistance = 1, const WorldPos& spawnPoint = WorldPos(0, 10, 0), unsigned int maxWorldChunkRadius = 6);
    ~World();
    
    // Core world operations
    void Update();
    void Render();
    void GenerateWorld();

    // Block operations
    Block GetBlock(const WorldPos& pos);
    Block GetBlock(int x, int y, int z);
    void SetBlock(const WorldPos& pos, const Block& block);
    void SetBlock(int x, int y, int z, const Block& block);
    
    // Chunk management
    Chunk* GetChunk(const ChunkCoord& coord);
    Chunk* GetOrCreateChunk(const ChunkCoord& coord);
    void LoadChunk(const ChunkCoord& coord);
    void UnloadChunk(const ChunkCoord& coord);
    bool IsChunkLoaded(const ChunkCoord& coord) const;

    // World limits
    unsigned int GetMaxWorldChunkRadius() const { return maxWorldChunkRadius; }
    void SetMaxWorldChunkRadius(unsigned int radius) { maxWorldChunkRadius = radius; }

    // World streaming (based on player position)
    void SetRenderDistance(unsigned int distance) { renderDistance = distance; }
    unsigned int GetRenderDistance() const { return renderDistance; }
    void SetPlayer(Player* player) { this->player = player; }
    Player* GetPlayer() const { return player; }
    void UpdateChunkLoading();
    
    // Rendering support
    std::vector<Chunk*> GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const;
    void BatchInvalidateChunkFaceNeighborMeshes(const ChunkCoord& coord);  // Prevents chain reactions
    void RegenerateDirtyChunks();  // Batch regenerate all dirty chunks
    
    // World queries
    WorldPos GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldPos& pos) { spawnPoint = pos; }

private:
    // Chunk storage
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> loadedChunks;
    
    // Terrain generation
    std::unique_ptr<TerrainGenerator> terrainGenerator;
    
    // World settings
    unsigned int renderDistance;
    WorldPos spawnPoint;
    Player* player; // Reference to the current player for chunk streaming
    unsigned int maxWorldChunkRadius;
    
    // Internal methods
    void UpdateChunkNeighbors(const ChunkCoord& coord, bool markNeighborsDirty = true);
    ChunkCoord WorldPosToChunkCoord(const WorldPos& pos) const;
    glm::ivec3 WorldPosToLocalChunkPos(const WorldPos& pos) const;
    std::vector<ChunkCoord> GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const;
    bool IsChunkOutsideWorld(const ChunkCoord& coord) const;
    void SetBlockQuiet(int x, int y, int z, const Block& block, std::unordered_set<Chunk*>& affectedChunks); // Set block without triggering invalidation

    static const ChunkCoord FACE_NEIGHBOR_OFFSETS[6];
    static constexpr int MAX_REGENERATIONS_PER_FRAME = 200;
};
