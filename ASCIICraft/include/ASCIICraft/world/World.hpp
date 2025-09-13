#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Chunk.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertexShader.hpp>

// Forward declarations
class Player;

// Main World class
class World {
public:
    World(unsigned int renderDistance = 2, const WorldPos& spawnPoint = WorldPos(0, 10, 0), unsigned int maxWorldChunkRadius = 2);
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
    
    // Manual chunk generation (for now - no automatic terrain)
    void GenerateEmptyChunk(const ChunkCoord& coord);
    
    // Rendering support
    std::vector<Chunk*> GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const;
    void BatchInvalidateChunkMeshes(const ChunkCoord& coord);  // Prevents chain reactions
    void RegenerateDirtyChunks();  // Batch regenerate all dirty chunks
    
    // World queries
    WorldPos GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldPos& pos) { spawnPoint = pos; }

private:
    // Chunk storage
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> loadedChunks;
    
    // World settings
    unsigned int renderDistance;
    WorldPos spawnPoint;
    Player* player; // Reference to the current player for chunk streaming
    unsigned int maxWorldChunkRadius;

    // rendering
    VERTEX_SHADER vertex_shader;
    
    // Internal methods
    void UpdateChunkNeighbors(const ChunkCoord& coord);
    ChunkCoord WorldPosToChunkCoord(const WorldPos& pos) const;
    glm::ivec3 WorldPosToLocalChunkPos(const WorldPos& pos) const;
    std::vector<ChunkCoord> GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const;
    bool IsChunkOutsideWorld(const ChunkCoord& coord) const;

    // Constants for neighbor directions (indices into NEIGHBOR_OFFSETS)
    static const ChunkCoord NEIGHBOR_OFFSETS[6];
};
