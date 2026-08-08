#pragma once

#include "support/ManualChunkJobScheduler.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>
#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>
#include <ASCIICraft/world/terrain/TerrainResult.hpp>

#include <entt/entt.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace testsupport {

/// A real ChunkManager driven frame by frame, with time and job execution under the
/// test's control.
///
/// What is real: ChunkManager::Update, the terrain generator, RegionFile and the
/// on-disk format, the entt registry, and a genuine player entity that ChunkManager
/// locates the same way the game does.
///
/// What is substituted: job execution (a manual queue), the clock (a counter), the
/// region directory (a temp dir), and rendering (never called).
///
/// The player is positioned, not simulated - no physics, no input. Movement exists to
/// drive chunk load/unload, which is the only way the streaming bugs can occur.
class WorldTestHarness {
public:
    struct Config {
        uint64_t seed = 12345ULL;
        /// Small on purpose: loadDistance is renderDistance + 1, so 1 gives a 5x5x5
        /// shell of 125 chunks. Larger values make the integrity walk very slow.
        unsigned int renderDistance = 1;
        glm::vec3 playerPos{8.0f, 88.0f, 8.0f};
        sizes::WorldDimensions dims{1024, 0, 1024};
        /// See ManualChunkJobScheduler::SetPolicy - mesh jobs are unbounded headless.
        bool dropMeshJobs = true;
        std::string label = "harness";
    };

    explicit WorldTestHarness(Config config = {});
    ~WorldTestHarness();

    entt::registry& Registry() { return registry_; }
    ChunkManager& Chunks() { return *chunkManager_; }
    ManualChunkJobScheduler& Scheduler() { return *scheduler_; }
    const std::filesystem::path& RegionDir() const { return dir_.Path(); }
    /// Keeps the temp region directory on disk for post-mortem inspection.
    void KeepRegionDir() { dir_.Keep(); }

    // --- player ---
    void MovePlayerTo(const glm::vec3& position);
    void MovePlayerToChunk(ChunkCoord coord);
    ChunkCoord PlayerChunk() const;

    // --- clock ---
    uint32_t Now() const { return clock_; }
    void AdvanceClock(uint32_t seconds) { clock_ += seconds; }

    // --- frame stepping ---

    /// One frame: ChunkManager::Update(), then optionally drain pending jobs.
    ///
    /// Draining runs terrain before unload rather than plain FIFO. That ordering is
    /// required, not cosmetic: FIFO lets an unload free a chunk whose terrain job is
    /// still later in the same batch, which corrupts the heap under defect A. Tests
    /// that want the adversarial ordering call Scheduler().RunFirst directly.
    void Step(bool pumpJobs = true);
    void StepFrames(int count, bool pumpJobs = true);

    /// Steps until no jobs are pending and every loaded chunk is generated.
    /// \returns false if it did not settle within \p maxFrames.
    bool Quiesce(int maxFrames = 64);

    /// Runs a deterministic random slice of the queue, terrain before unload.
    ///
    /// Reproduces the real interleaving where some terrain lands and some is still
    /// pending when the next unload arrives, without the terrain-after-free ordering
    /// that crashes under defect A. That ordering has its own dedicated pins.
    void PumpRandomSubset(uint64_t& rngState, double fraction);

    /// Discards queued terrain jobs whose target chunk is no longer loaded.
    /// \returns how many were dropped.
    ///
    /// This is a WORKAROUND FOR DEFECT A, not a convenience. Terrain jobs capture a
    /// raw Chunk* and nothing cancels them on unload, so running one after its chunk
    /// was freed writes 4096 state ids through a dangling pointer. That reliably
    /// corrupts the heap and kills the process - verified, exit 0xC0000374.
    ///
    /// A crashed test cannot be a regression pin, so tests that need to exercise
    /// defects B or C call this first to neutralise A. Once A is fixed the call
    /// becomes a no-op and can be deleted.
    size_t DropStaleTerrainJobs();

    /// Quiesce, dropping stale terrain jobs between frames. Safe under defect A.
    bool QuiesceSafely(int maxFrames = 64);

    // --- reference data ---

    /// Terrain for \p coord generated standalone, bypassing the chunk manager
    /// entirely. This is the ground truth every content assertion compares against.
    std::vector<uint32_t> GenerateReference(ChunkCoord coord,
                                            TerrainResult* crossOut = nullptr);

    /// Copy of a loaded chunk's blocks, or empty if it is not loaded.
    std::vector<uint32_t> BlocksOf(ChunkCoord coord) const;

private:
    Config config_;
    TempDir dir_;
    entt::registry registry_;
    entt::entity player_ = entt::null;
    uint32_t clock_ = 1000;   // non-zero so `now - lastTouched` cannot underflow
    /// Drains pending jobs terrain-first; see Step().
    void PumpOrdered();

    ManualChunkJobScheduler* scheduler_ = nullptr;   // owned by the ChunkManager
    std::unique_ptr<ChunkManager> chunkManager_;
    std::unique_ptr<TerrainGenerator> referenceGenerator_;
};

} // namespace testsupport
