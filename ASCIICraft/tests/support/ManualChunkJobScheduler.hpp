#pragma once

#include <ASCIICraft/world/chunk/IChunkJobScheduler.hpp>

#include <cstddef>
#include <deque>
#include <functional>
#include <vector>

namespace testsupport {

/// A scheduler that never runs anything on its own.
///
/// This is what turns the chunk manager's races into tests. Jobs are held in a queue
/// until the test explicitly runs them, so a test can execute an unload BEFORE the
/// terrain job it raced with, every time, on every machine. No threads, no sleeps,
/// no flakes.
class ManualChunkJobScheduler final : public IChunkJobScheduler {
public:
    enum class Policy {
        Queue,      ///< hold until the test runs it (default)
        Drop,       ///< discard on submission
        RunInline,  ///< execute immediately inside Run()
    };

    /// Policy for a job kind.
    ///
    /// Mesh jobs default to Drop, and that is load-bearing rather than tidy-minded.
    /// Headless there is no terrainTextureArray, so ApplyDrainedMeshResults early-
    /// returns, chunks stay dirty forever, and EnqueueMeshForDirtyChunks re-enqueues
    /// every eligible chunk every frame. Each mesh job pins ~114 KiB of block copies,
    /// so a few hundred frames would queue gigabytes.
    void SetPolicy(ChunkJobKind kind, Policy policy);
    Policy GetPolicy(ChunkJobKind kind) const;

    // --- IChunkJobScheduler ---
    void Run(ChunkJobTag tag, std::function<void()> task) override;
    void Wait() override;

    // --- test control surface ---

    size_t PendingCount() const;
    size_t CountPending(ChunkJobKind kind) const;
    size_t CountPending(ChunkJobKind kind, ChunkCoord coord) const;
    std::vector<ChunkJobTag> PendingTags() const;

    /// Runs queued jobs FIFO until none remain, including jobs spawned by jobs.
    /// \param maxJobs safety valve against a job that re-enqueues itself forever.
    void RunAll(size_t maxJobs = 100000);

    void RunAllOfKind(ChunkJobKind kind);

    /// Runs the oldest job matching (kind, coord), out of order. Returns false if
    /// there is none. This is the primitive that reproduces the lifecycle races.
    bool RunFirst(ChunkJobKind kind, ChunkCoord coord);

    /// Runs the oldest job of this kind regardless of coord.
    bool RunFirstOfKind(ChunkJobKind kind);

    /// Discards without running - simulates a cancellation the real queue cannot do.
    bool DropFirst(ChunkJobKind kind, ChunkCoord coord);
    size_t DropAllOfKind(ChunkJobKind kind);

    /// Runs a deterministic pseudo-random subset, for the integrity walk.
    /// \param rngState advanced in place; same seed gives the same schedule.
    ///
    /// Runs jobs in arbitrary order, so it CAN run a terrain job after the unload that
    /// freed its chunk - which crashes under defect A. Use RunRandomSubsetOfKind in
    /// terrain-then-unload order unless you specifically want that ordering.
    void RunRandomSubset(uint64_t& rngState, double fraction);

    /// As RunRandomSubset, restricted to one job kind. Calling it per kind in
    /// terrain -> mesh -> unload order keeps partial pumping while ensuring a terrain
    /// job never runs after its chunk was freed in the same pass.
    void RunRandomSubsetOfKind(ChunkJobKind kind, uint64_t& rngState, double fraction);

    /// Total jobs executed since construction, by kind.
    size_t ExecutedCount(ChunkJobKind kind) const;

private:
    struct Job {
        ChunkJobTag tag;
        std::function<void()> task;
    };

    Policy PolicyFor(ChunkJobKind kind) const;

    std::deque<Job> pending_;
    Policy policies_[3] = { Policy::Queue, Policy::Drop, Policy::Queue };  // Terrain, Mesh, Unload
    size_t executed_[3] = { 0, 0, 0 };
    /// Guards against re-entrancy: a running job may enqueue more work.
    bool running_ = false;
};

} // namespace testsupport
