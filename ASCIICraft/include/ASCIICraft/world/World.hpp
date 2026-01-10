#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/TerrainGenerator.hpp>
#include <ASCIICraft/world/CrossChunkEdit.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/ChunkRegion.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <iterator>

#include <glm/glm.hpp>

// Forward declarations
class Player;

// Main World class
class World {
public:
    World(unsigned int renderDistance = 1, const WorldCoord& spawnPoint = WorldCoord(0, 10, 0), unsigned int maxWorldChunkRadius = 6);
    ~World();
    
    // Core world operations
    void Update();
    void Render();
    void GenerateWorld();

    // Block operations
    Block GetBlock(const WorldCoord& pos);
    Block GetBlock(int x, int y, int z);
    void SetBlock(const WorldCoord& pos, const Block& block);
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
    WorldCoord GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldCoord& pos) { spawnPoint = pos; }

private:
    // Chunk storage and management
    std::unique_ptr<RegionManager> regionManager;

    // make sure to flush edits on world save / shutdown
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>> loadedChunks;
    std::unordered_map<ChunkCoord, MetaBucket> crossChunkEdits;

    // queue to track metaBucket lifetimes
    std::queue<ChunkCoord> metaTimeTracker;

    // Terrain generation
    std::unique_ptr<TerrainGenerator> terrainGenerator;
    
    // World settings
    unsigned int renderDistance;
    WorldCoord spawnPoint;
    Player* player; // Reference to the current player for chunk streaming
    unsigned int maxWorldChunkRadius;
    
    // Internal methods
    void UpdateChunkNeighbors(const ChunkCoord& coord, bool markNeighborsDirty = true);
    std::vector<ChunkCoord> GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const;
    bool IsChunkOutsideWorld(const ChunkCoord& coord) const;

    static const ChunkCoord FACE_NEIGHBOR_OFFSETS[6];
    static constexpr int MAX_REGENERATIONS_PER_FRAME = 200;
};
