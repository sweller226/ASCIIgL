// Chunk lifecycle - the reported bugs, reproduced deterministically.
//
// Every race here is made repeatable by choosing job order rather than observing it.
// No threads, no sleeps: RunFirst picks the exact interleaving, so these fail the same
// way on every machine and every run.
//
//   A  terrain jobs hold a raw Chunk* with no unload guard   -> "chunk full of dirt"
//   B  ungenerated chunks are persisted and never regenerated -> "flowers missing"
//   C  stale results are applied by coordinate, not identity  -> "trees cut off"

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/WorldTestHarness.hpp"

#include <ASCIICraft/world/chunk/ChunkRegion.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

#include <algorithm>
#include <memory>

using testsupport::WorldTestHarness;

namespace {

// Not constexpr: ChunkCoord's constructors are not constexpr, so it is not a literal type.
const ChunkCoord kTarget{0, 5, 0};
const ChunkCoord kFarAway{200, 5, 200};

/// True when the chunk holds something other than a single repeated block. A chunk
/// that was persisted before generating comes back uniformly air.
bool HasVariedContent(const std::vector<uint32_t>& blocks) {
    if (blocks.empty()) return false;
    const uint32_t first = blocks[0];
    return std::any_of(blocks.begin(), blocks.end(),
                       [first](uint32_t v) { return v != first; });
}

bool IsAllAir(const std::vector<uint32_t>& blocks) {
    if (blocks.empty()) return false;
    const uint32_t air = testsupport::Ids().air;
    return std::all_of(blocks.begin(), blocks.end(),
                       [air](uint32_t v) { return v == air; });
}

} // namespace

TEST_SUITE("world.tier3.lifecycle") {

// --- DEFECT A: no guard between unload and an in-flight terrain job ----------

TEST_CASE("unloading a chunk cancels its pending terrain job"
          * doctest::should_fail()) {
    // EnqueueTerrainGen captures a raw Chunk*. UnloadChunk drops the shared_ptr and
    // hands the last reference to an unload job in the same unordered queue, with no
    // cancellation. The terrain job survives its target.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);

    CHECK(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 0);
}

TEST_CASE("a pending terrain job never outlives its chunk"
          * doctest::should_fail()) {
    // The use-after-free, demonstrated without invoking it.
    //
    // Hold a weak_ptr, force the unload job to run first, then observe that the chunk
    // is destroyed while a terrain job targeting it is still queued. Running that job
    // would write 4096 state ids through a dangling pointer - which, for an
    // underground chunk, is 4096 dirt blocks into whatever now owns that memory.
    // That is the "entirely filled with dirt" report.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    std::weak_ptr<const Chunk> watch = h.Chunks().GetChunkShared(kTarget);
    REQUIRE_FALSE(watch.expired());

    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, kTarget));

    const bool chunkDestroyed = watch.expired();
    const bool jobStillQueued = h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) > 0;
    CAPTURE(chunkDestroyed);
    CAPTURE(jobStillQueued);

    // Safe to leave the stale job queued: the scheduler is destroyed with it unrun.
    h.Scheduler().DropFirst(ChunkJobKind::Terrain, kTarget);

    CHECK_FALSE((chunkDestroyed && jobStillQueued));
}

// --- DEFECT B: ungenerated chunks are persisted ------------------------------

TEST_CASE("a chunk that never generated is not written to disk"
          * doctest::should_fail()) {
    // SaveChunkForUnload has no IsGenerated() guard, unlike SaveAll. An all-air chunk
    // is written with the present flag set.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    // Walk away before terrain runs, then let only the unload happen.
    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, kTarget));
    h.Scheduler().DropFirst(ChunkJobKind::Terrain, kTarget);

    // Read the region back directly - nothing should be present for this chunk.
    RegionFile region(kTarget.ToRegionCoord(), h.RegionDir());
    Chunk probe(kTarget);
    const bool present = region.LoadChunk(&probe, testsupport::SharedBlocks());
    CHECK_FALSE(present);
}

TEST_CASE("a chunk persisted before generating is regenerated on return"
          * doctest::should_fail()) {
    // The player-visible half of B, and the reported "chunks of flowers don't
    // generate": once an all-air chunk is on disk, LoadChunk succeeds, marks it
    // generated, and terrain is never enqueued again. The chunk stays empty forever.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, kTarget));
    h.Scheduler().DropFirst(ChunkJobKind::Terrain, kTarget);
    h.Scheduler().RunAll();

    // Come back and let everything settle.
    h.MovePlayerToChunk(kTarget);
    REQUIRE(h.Quiesce());

    const std::vector<uint32_t> blocks = h.BlocksOf(kTarget);
    REQUIRE(blocks.size() == static_cast<size_t>(Chunk::VOLUME));
    CAPTURE(IsAllAir(blocks));

    // A surface chunk must have real terrain, not the empty husk that was saved.
    CHECK(HasVariedContent(blocks));
}

TEST_CASE("churning the load boundary does not empty chunks"
          * doctest::should_fail()) {
    // Why the symptom appears in contiguous groups rather than isolated chunks.
    // UNLOAD_RADIUS_PADDING is 0, so there is no hysteresis: a player oscillating
    // across one chunk line loads and unloads an entire shell repeatedly, and every
    // chunk caught mid-generation gets persisted empty.
    WorldTestHarness h({});

    // Oscillate 6 chunks apart. With loadDistance 2 the two shells do not overlap, so
    // every chunk in one shell is genuinely unloaded when the player jumps to the
    // other - and any whose terrain has not run yet gets persisted empty.
    const ChunkCoord a{0, 5, 0};
    const ChunkCoord b{6, 5, 0};

    for (int i = 0; i < 8; ++i) {
        h.MovePlayerToChunk(a);
        h.Step(/*pumpJobs=*/false);   // load shell A, terrain queued but NOT run
        h.MovePlayerToChunk(b);
        h.Step(/*pumpJobs=*/false);   // shell A unloads with terrain still pending
        h.Scheduler().RunAllOfKind(ChunkJobKind::Unload);
        // Neutralise defect A - see WorldTestHarness::DropStaleTerrainJobs. Without
        // this the run dies of heap corruption before it can measure defect B.
        h.DropStaleTerrainJobs();
    }

    // Settle back over shell A and inspect what actually loaded.
    h.MovePlayerToChunk(a);
    REQUIRE(h.QuiesceSafely());

    size_t emptied = 0;
    size_t checked = 0;
    for (const ChunkCoord coord : h.Chunks().GetLoadedCoords()) {
        const auto chunk = h.Chunks().GetChunkShared(coord);
        if (!chunk || !chunk->IsGenerated()) continue;
        const std::vector<uint32_t> reference = h.GenerateReference(coord);
        if (!HasVariedContent(reference)) continue;   // only judge chunks with terrain
        ++checked;
        if (IsAllAir(h.BlocksOf(coord))) ++emptied;
    }
    CAPTURE(checked);
    CAPTURE(emptied);
    REQUIRE(checked > 0);
    CHECK(emptied == 0);
}

// --- DEFECT C: stale results applied by coordinate, not identity -------------

TEST_CASE("defect B masks defect C: a reloaded chunk gets no second terrain job") {
    // NOT a should_fail pin, because C cannot currently be reproduced - and the reason
    // is itself a finding worth locking down.
    //
    // C needs a chunk to have BOTH a stale result queued and its own fresh terrain job
    // pending, so the stale one can be misapplied to the new instance. That never
    // happens today: defect B persisted the ungenerated chunk, so on return LoadChunk
    // takes the disk-hit path, calls SetGenerated(true), and never enqueues terrain.
    //
    // Measured below: after unload and return there is exactly ONE terrain job (the
    // stale one) and the chunk already reports generated.
    //
    // Consequence for sequencing: fixing B will UNMASK C. The real C pin has to be
    // written at that point - do not treat C as resolved just because nothing here is
    // red for it.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    // Unload without running the terrain job, leaving a stale one queued.
    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, kTarget));
    REQUIRE(h.Chunks().GetChunkShared(kTarget) == nullptr);

    // Return. A new Chunk is created for the coord.
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);

    const auto chunk = h.Chunks().GetChunkShared(kTarget);
    REQUIRE(chunk != nullptr);

    const size_t terrainJobs = h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget);
    CAPTURE(terrainJobs);

    // The stale job is the only one, and the chunk is already "generated" from the
    // empty blob B wrote. Both halves of the masking, asserted.
    CHECK(terrainJobs == 1);
    CHECK(chunk->IsGenerated());

    h.Scheduler().DropAllOfKind(ChunkJobKind::Terrain);
}

TEST_CASE("a cross-chunk edit for an unloaded chunk is retained, not dropped") {
    // The buffering half of the tree-spill path, in isolation. This one passes today -
    // which matters, because it means a spill that goes missing is lost during APPLY
    // or PERSIST (defects C, D, E), not during buffering.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);

    // Far outside the load radius, so it cannot be applied directly.
    const ChunkCoord spillTarget = ChunkCoord{60, 5, 60};
    const uint32_t log = testsupport::Ids().oakLog;
    h.Chunks().SetBlockState(spillTarget.x * 16 + 2,
                             spillTarget.y * 16 + 3,
                             spillTarget.z * 16 + 4, log);

    REQUIRE(h.Chunks().HasPendingCrossChunkEdits(spillTarget));
    const auto edits = h.Chunks().GetPendingCrossChunkEdits(spillTarget);
    REQUIRE(edits.size() == 1);
    CHECK(edits[0].stateId == log);

    // It must still be there after unrelated frames elapse.
    h.StepFrames(5);
    CHECK(h.Chunks().HasPendingCrossChunkEdits(spillTarget));
}

// --- DEFECT G: a chunk whose region cannot open blocks its neighbours --------

TEST_CASE("a chunk is never left permanently ungenerated"
          * doctest::should_fail()) {
    // LoadChunk inserts into loadedChunks and only then checks the region. On failure
    // it returns, leaving an ungenerated chunk in the map forever. AllNeighborsGenerated
    // then blocks meshing for all six neighbours indefinitely.
    //
    // Reproduced without a filesystem failure: any chunk that is loaded but whose
    // terrain never lands has the same effect on its neighbours.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);

    // Drop one chunk's terrain job outright - the queue offers no cancellation, so
    // this stands in for any path where a result never arrives.
    REQUIRE(h.Scheduler().DropFirst(ChunkJobKind::Terrain, kTarget));
    h.Scheduler().RunAll();
    h.StepFrames(8);

    const auto stats = h.Chunks().GetStats();
    CAPTURE(stats.loadedChunks);
    CAPTURE(stats.generatedChunks);

    // Either it recovers, or the chunk should not still be sitting there ungenerated.
    const auto chunk = h.Chunks().GetChunkShared(kTarget);
    const bool stuck = chunk && !chunk->IsGenerated();
    CHECK_FALSE(stuck);
}

} // TEST_SUITE("world.tier3.lifecycle")
