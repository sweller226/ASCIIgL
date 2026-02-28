#pragma once

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/ChunkMeshGen.hpp>
#include <ASCIICraft/world/TerrainResult.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <memory>

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

/// Job queue for chunk terrain generation and mesh generation using oneTBB.
/// - Takes registry to get BlockStateRegistry from context when enqueueing.
/// - EnqueueTerrainGen(Chunk*) / EnqueueMeshGen(Chunk*) copy what they need from the chunk;
///   workers never touch Chunk* after the enqueue step.
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

    /// Enqueue terrain generation for \p chunk. Block data is generated on a worker and
    /// returned via DrainCompletedTerrainResults; apply the block data to the chunk on the main thread.
    void EnqueueTerrainGen(Chunk* chunk);

    /// Enqueue mesh generation for \p chunk. Reads chunk and neighbor blocks (and BSR from registry)
    /// and builds mesh data on a worker; result returned via DrainCompletedMeshResults.
    void EnqueueMeshGen(Chunk* chunk);

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

    /// Wait for all currently enqueued jobs to complete. Use before shutdown or when pausing.
    void WaitForPending();

private:
    entt::registry& registry_;
    TerrainGenerator* terrainGenerator_ = nullptr;

    oneapi::tbb::task_group taskGroup_;
    oneapi::tbb::concurrent_queue<CompletedTerrainResult> completedTerrainQueue_;
    oneapi::tbb::concurrent_queue<CompletedMeshResult> completedMeshQueue_;
    size_t maxDrainPerFrame_ = 0;
    size_t maxDrainMeshPerFrame_ = 0;
};
