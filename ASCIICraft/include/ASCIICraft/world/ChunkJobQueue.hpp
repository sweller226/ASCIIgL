#pragma once

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/ChunkMeshGen.hpp>
#include <ASCIICraft/world/TerrainResult.hpp>
#include <ASCIICraft/world/CrossChunkEdit.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>
#include <functional>

#include <entt/entt.hpp>

#include <oneapi/tbb/task_group.h>
#include <oneapi/tbb/concurrent_queue.h>

class TerrainGenerator;

/// Result pushed when a mesh job finishes.
struct CompletedMeshResult {
    ChunkCoord coord;
    ChunkMeshData data;
};

/// Result pushed when a terrain generation job finishes (terrain data to apply to chunk).
struct CompletedTerrainResult {
    ChunkCoord coord;
    TerrainResult result;
};

/// Callback run on unload task: save chunk and optional metadata. ChunkManager holds unloadMutex_ during save.
using UnloadSaveCallback = std::function<void(Chunk* chunk, ChunkCoord coord, const MetaBucket* meta)>;

/// Job queue for chunk terrain generation, mesh generation, and chunk unloading using oneTBB.
/// - Takes registry to get BlockStateRegistry from context when enqueueing.
/// - EnqueueTerrainGen(Chunk*): worker writes terrain directly into the chunk.
/// - EnqueueMeshGen(Chunk*): copies chunk + neighbor blocks for the worker; workers do not touch Chunk* after enqueue.
/// - Drain completed results on the main thread and apply (apply block data to chunk, or create Mesh and assign).
class ChunkJobQueue {
public:
    explicit ChunkJobQueue(entt::registry& registry);
    ~ChunkJobQueue();

    ChunkJobQueue(const ChunkJobQueue&) = delete;
    ChunkJobQueue& operator=(const ChunkJobQueue&) = delete;

    /// Set the terrain generator used by EnqueueTerrainGen. Must be set before enqueueing terrain jobs.
    void SetTerrainGenerator(TerrainGenerator* gen) { terrainGenerator_ = gen; }
    TerrainGenerator* GetTerrainGenerator() const { return terrainGenerator_; }

    void EnqueueTerrainGen(Chunk* chunk);
    void EnqueueMeshGen(Chunk* chunk);
    void EnqueueUnload(ChunkCoord coord, std::shared_ptr<Chunk> chunk, std::optional<MetaBucket> meta);

    /// Set callback invoked on the unload task to perform region SaveChunk/SaveMetaData. Required for EnqueueUnload.
    void SetUnloadSaveCallback(UnloadSaveCallback cb) { unloadSaveCallback_ = std::move(cb); }

    /// Drain completed terrain results into \p out (cleared first). Reuse \p out each frame to avoid allocs.
    void DrainCompletedTerrainResultsInto(std::vector<CompletedTerrainResult>& out);
    /// Drain completed mesh results into \p out (cleared first). Reuse \p out each frame to avoid allocs.
    void DrainCompletedMeshResultsInto(std::vector<CompletedMeshResult>& out);

    /// Optional: limit how many terrain results are drained per call. 0 = no limit (default).
    void SetMaxDrainPerFrame(size_t maxCount) { maxDrainPerFrame_ = maxCount; }
    size_t GetMaxDrainPerFrame() const { return maxDrainPerFrame_; }
    /// Optional: limit how many mesh results are drained per call (GPU uploads on main thread). 0 = no limit.
    void SetMaxDrainMeshPerFrame(size_t maxCount) { maxDrainMeshPerFrame_ = maxCount; }
    size_t GetMaxDrainMeshPerFrame() const { return maxDrainMeshPerFrame_; }

    /// Wait for all currently enqueued jobs (terrain, mesh, unload) to complete. Use before shutdown or when pausing.
    void WaitForPending();

private:
    entt::registry& registry_;
    TerrainGenerator* terrainGenerator_ = nullptr;
    UnloadSaveCallback unloadSaveCallback_;

    oneapi::tbb::task_group taskGroup_;
    oneapi::tbb::concurrent_queue<CompletedTerrainResult> completedTerrainQueue_;
    oneapi::tbb::concurrent_queue<CompletedMeshResult> completedMeshQueue_;

    size_t maxDrainPerFrame_ = 0;
    size_t maxDrainMeshPerFrame_ = 0;
};
