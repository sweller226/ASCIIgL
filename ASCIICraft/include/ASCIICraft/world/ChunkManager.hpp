#pragma once

#include <unordered_map>
#include <queue>
#include <memory>
#include <mutex>

#include <ASCIICraft/world/ChunkRegion.hpp>
#include <ASCIICraft/world/ChunkJobQueue.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/TerrainGenerator.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <entt/entt.hpp>

#include <glm/glm.hpp>

/// Fog parameters used when rendering chunks. fogStart/fogEnd are derived from render distance.
/// fogColor is sRGB in [0,1]; the terrain shader linearizes it before blending in linear space.
struct ChunkManagerFogParams {
    float fogStart = 0.f;
    float fogEnd = 0.f;
    glm::vec3 fogColor = glm::vec3(0.f);  // sRGB 0-1, matches palette/gradient convention
};

class ChunkManager {
public:
    ChunkManager(entt::registry& registry, const unsigned int chunkWorldLimit, const unsigned int renderDistance);
    ~ChunkManager() = default; // default is fine; list elements are destroyed automatically

    void Update();

    // Block operations (blockstate IDs)
    uint32_t GetBlockState(const WorldCoord& pos) const;
    uint32_t GetBlockState(int x, int y, int z) const;
    void SetBlockState(const WorldCoord& pos, uint32_t stateId);
    void SetBlockState(int x, int y, int z, uint32_t stateId);

    // Save handling
    void SaveAll();

    // World limits
    unsigned int GetMaxWorldChunkRadius() const { return maxWorldChunkRadius; }
    void SetMaxWorldChunkRadius(unsigned int radius) { maxWorldChunkRadius = radius; }
    
    // Rendering support
    void RenderChunks();
    
    // World streaming (based on player position)
    void SetRenderDistance(unsigned int distance);
    unsigned int GetRenderDistance() const { return renderDistance; }

    // Fog (used in RenderChunks; start/end are tied to render distance)
    ChunkManagerFogParams& GetFogParams() { return fogParams_; }
    const ChunkManagerFogParams& GetFogParams() const { return fogParams_; }
    void SetFogParams(const ChunkManagerFogParams& p) { fogParams_ = p; }
    /// color: sRGB in [0,1] (shader linearizes before blending)
    void SetFogColor(const glm::vec3& color) { fogParams_.fogColor = color; }

    // Raycasting (returns stateId and position)
    std::pair<uint32_t, WorldCoord> BlockIntersectsView(glm::vec3& lookDir, glm::vec3& headPos, float reach);
    std::pair<bool, WorldCoord> BlockIntersectsViewForPlacement(glm::vec3& lookDir, glm::vec3& headPos, float reach);
    
    void BlockUpdateNeighboursDirty(const ChunkCoord& chunkCoord, const glm::ivec3& localPos);
private:
    entt::registry& registry;

    // Chunk storage and management
    std::unique_ptr<RegionManager> regionManager;

    // make sure to flush edits on world save / shutdown
    std::unordered_map<ChunkCoord, std::shared_ptr<Chunk>> loadedChunks;
    std::mutex unloadMutex_;  // serializes unload save callback (region I/O)
    std::unordered_map<ChunkCoord, MetaBucket> crossChunkEdits;

    // queue to track metaBucket lifetimes
    std::queue<ChunkCoord> metaTimeTracker;

    // Terrain generation
    TerrainGenerator terrainGenerator;

    // Chunk job queue (terrain + mesh on worker threads)
    std::unique_ptr<ChunkJobQueue> chunkJobQueue;
    // Reused each frame to avoid allocs when draining job results
    std::vector<CompletedTerrainResult> drainTerrainBuffer_;
    std::vector<CompletedMeshResult> drainMeshBuffer_;

    // Internal methods
    /// Wire neighbor pointers for chunk at coord; mark each neighbor dirty when both have terrain (so edge chunks re-mesh).
    void UpdateChunkNeighbors(const ChunkCoord& coord);
    /// True iff every in-world neighbor is loaded and has terrain (IsGenerated). Edge chunks do not mesh until neighbors past them are loaded.
    bool AllNeighborsGenerated(const ChunkCoord& coord) const;
    std::vector<ChunkCoord> GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const;
    bool IsChunkOutsideWorld(const ChunkCoord& coord) const;

    // chunk rendering support
    std::vector<Chunk*> GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const;
    void BatchInvalidateChunkFaceNeighborMeshes(const ChunkCoord& coord);  // Prevents chain reactions
    void UpdateFogFromRenderDistance();  // sets fogParams_.fogStart, fogParams_.fogEnd from renderDistance

    // Chunk management
    RegionFile& GetOrCreateRegion(const RegionCoord& rp);
    Chunk* GetChunk(const ChunkCoord& coord);
    Chunk* GetOrCreateChunk(const ChunkCoord& coord);
    void LoadChunk(const ChunkCoord& coord);
    void UnloadChunk(const ChunkCoord& coord);
    bool IsChunkLoaded(const ChunkCoord& coord) const;
    void UpdateChunkLoading();
    void ProcessMetaBucketExpiry();
    void LoadChunksInRadius(const ChunkCoord& playerChunk, unsigned int loadRadius);
    void UnloadDistantChunks(const ChunkCoord& playerChunk, unsigned int unloadRadius);
    void DrainAndApplyJobResults();
    void ApplyDrainedTerrainResults();
    void ApplyDrainedMeshResults();
    void EnqueueMeshForDirtyChunks();
    /// Rebuild mesh on main thread and apply immediately (for same-frame block-edit feedback).
    void RebuildChunkMeshImmediate(Chunk* c);
    /// Apply a list of cross-chunk edits to a chunk (local coords). Used when applying terrain meta and when loading from file.
    void ApplyEditsToChunk(Chunk* c, const std::vector<CrossChunkEdit>& edits);

    static const ChunkCoord FACE_NEIGHBOR_OFFSETS[6];
    /// Tuned for 16x16x16 chunks (more chunks per volume than tall column chunks).
    static constexpr int MAX_QUEUES_PER_FRAME = 64;
    static constexpr int MAX_CHUNK_LOADS_PER_FRAME = 32; // spread disk I/O; higher ok for small chunks
    static constexpr int MAX_CHUNK_UNLOADS_PER_FRAME = 32; // cap unloads per frame; rest drained next frame
    static constexpr int MAX_MESH_APPLIES_PER_FRAME = 32;  // GPU uploads per frame; small meshes = cheaper
    static constexpr int MAX_SYNC_MESH_REBUILDS_PER_FRAME = 4;  // main-thread mesh build (small chunk = fast)

    // World settings
    unsigned int maxWorldChunkRadius;
    unsigned int renderDistance;

    ChunkManagerFogParams fogParams_;
};
