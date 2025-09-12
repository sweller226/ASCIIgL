#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/player/Player.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertexShader.hpp>

// Main World class
class World {
public:
    World(unsigned int renderDistance = 2, const WorldPos& spawnPoint = WorldPos(0, 10, 0));
    ~World();
    
    // Core world operations
    void Update(float deltaTime);
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
    
    // World streaming (based on player position)
    void SetRenderDistance(int distance) { renderDistance = distance; }
    int GetRenderDistance() const { return renderDistance; }
    void SetPlayer(Player* player) { this->player = player; }
    Player* GetPlayer() const { return player; }
    void UpdateChunkLoading();
    
    // Manual chunk generation (for now - no automatic terrain)
    void GenerateEmptyChunk(const ChunkCoord& coord);
    
    // Rendering support
    std::vector<Chunk*> GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const;
    void InvalidateChunkMeshes(const ChunkCoord& coord);  // For block changes
    
    // World queries
    WorldPos GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldPos& pos) { spawnPoint = pos; }

private:
    // Chunk storage
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> loadedChunks;
    
    // World settings
    int renderDistance;
    WorldPos spawnPoint;
    Player* player; // Reference to the current player for chunk streaming

    // rendering
    VERTEX_SHADER vertex_shader;
    
    // Internal methods
    void UpdateChunkNeighbors(const ChunkCoord& coord);
    ChunkCoord WorldPosToChunkCoord(const WorldPos& pos) const;
    glm::ivec3 WorldPosToLocalChunkPos(const WorldPos& pos) const;
    std::vector<ChunkCoord> GetChunksInRadius(const ChunkCoord& center, int radius) const;
    
    // Constants for neighbor directions
    enum NeighborDirection {
        NEIGHBOR_POS_X = 0,
        NEIGHBOR_NEG_X = 1,
        NEIGHBOR_POS_Y = 2,
        NEIGHBOR_NEG_Y = 3,
        NEIGHBOR_POS_Z = 4,
        NEIGHBOR_NEG_Z = 5
    };
    
    static const ChunkCoord NEIGHBOR_OFFSETS[6];
};
