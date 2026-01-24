#include <unordered_map>
#include <queue>
#include <memory>

#include <ASCIICraft/world/ChunkRegion.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/TerrainGenerator.hpp>

class ChunkManager {
public:
    ChunkManager() = default;
    ~ChunkManager() = default; // default is fine; list elements are destroyed automatically

    void Update();

    // Block operations
    Block GetBlock(const WorldCoord& pos);
    Block GetBlock(int x, int y, int z);
    void SetBlock(const WorldCoord& pos, const Block& block);
    void SetBlock(int x, int y, int z, const Block& block);

    // World limits
    unsigned int GetMaxWorldChunkRadius() const { return maxWorldChunkRadius; }
    void SetMaxWorldChunkRadius(unsigned int radius) { maxWorldChunkRadius = radius; }
    
    // Rendering support
    std::vector<Chunk*> GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const;
    void BatchInvalidateChunkFaceNeighborMeshes(const ChunkCoord& coord);  // Prevents chain reactions
    void RegenerateDirtyChunks();  // Batch regenerate all dirty chunks
    
    // World streaming (based on player position)
    void SetRenderDistance(unsigned int distance) { renderDistance = distance; }
    unsigned int GetRenderDistance() const { return renderDistance; }

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

    // Internal methods
    void UpdateChunkNeighbors(const ChunkCoord& coord, bool markNeighborsDirty = true);
    std::vector<ChunkCoord> GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const;
    bool IsChunkOutsideWorld(const ChunkCoord& coord) const;

    // chunk generation management
    // void GenerateWorld();

    // Chunk management
    Chunk* GetChunk(const ChunkCoord& coord);
    Chunk* GetOrCreateChunk(const ChunkCoord& coord);
    void LoadChunk(const ChunkCoord& coord);
    void UnloadChunk(const ChunkCoord& coord);
    bool IsChunkLoaded(const ChunkCoord& coord) const;
    void UpdateChunkLoading();

    static const ChunkCoord FACE_NEIGHBOR_OFFSETS[6];
    static constexpr int MAX_REGENERATIONS_PER_FRAME = 200;

    // World settings
    unsigned int maxWorldChunkRadius;
    unsigned int renderDistance;
};
