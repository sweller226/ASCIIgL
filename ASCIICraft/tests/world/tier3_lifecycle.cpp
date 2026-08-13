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

TEST_CASE("unloading a chunk makes its pending terrain job inert") {
    // Originally asserted CountPending == 0, which was the wrong invariant: oneTBB's
    // task_group cannot withdraw a submitted task, so no production scheduler can
    // actually dequeue. What is achievable - and what matters - is that a job left in
    // the queue is harmless: it must not write into freed memory, and its result must
    // not be applied to whatever occupies the coord later.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    std::weak_ptr<const Chunk> watch = h.Chunks().GetChunkShared(kTarget);
    REQUIRE_FALSE(watch.expired());

    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, kTarget));

    // The chunk outlives the unload because the queued job holds a reference. That is
    // what makes running it safe rather than a use-after-free.
    CHECK_FALSE(watch.expired());

    // Running the stale job must neither crash nor produce anything the manager acts on.
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Terrain, kTarget));
    h.Chunks().Update();
    CHECK(h.Chunks().GetChunkShared(kTarget) == nullptr);   // still unloaded

    // Once the last reference goes, the chunk is reclaimed - no leak.
    h.Scheduler().RunAll();
    CHECK(watch.expired());
}

TEST_CASE("a pending terrain job never outlives its chunk") {
    // Previously the use-after-free: the unload job dropped the last reference while a
    // terrain job still held a raw pointer to the chunk, and running it wrote 4096
    // state ids into freed memory - 4096 dirt blocks, for an underground chunk, into
    // whatever now owned that allocation. That was the "entirely filled with dirt"
    // report, and it reproduced as heap corruption (exit 0xC0000374).
    //
    // Now the job holds a shared_ptr, so the chunk cannot be destroyed while a job
    // targeting it is queued.
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

TEST_CASE("a chunk that never generated is not written to disk") {
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

TEST_CASE("a chunk persisted before generating is regenerated on return") {
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

TEST_CASE("churning the load boundary does not empty chunks") {
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
    }

    // Settle back over shell A and inspect what actually loaded.
    h.MovePlayerToChunk(a);
    REQUIRE(h.Quiesce());

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

// --- DEFECT C: stale results applied by coordinate, not identity -------------

TEST_CASE("a stale terrain result is not applied to a re-created chunk") {
    // This scenario was UNREACHABLE until defect B was fixed. B persisted the
    // ungenerated chunk, so returning to it took the disk-hit path, marked it
    // generated, and never enqueued terrain - there was never a second job for a stale
    // result to race. With B fixed the reload enqueues its own job, and both exist at
    // once for the first time.
    //
    // The fix is identity matching: CompletedTerrainResult carries the instanceId of
    // the Chunk it was generated for, and the drain drops results whose instance no
    // longer occupies the coord.
    WorldTestHarness h({});
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) == 1);

    const auto original = h.Chunks().GetChunkShared(kTarget);
    REQUIRE(original != nullptr);
    const uint64_t originalId = original->GetInstanceId();

    // Unload without running the terrain job, leaving a stale one queued.
    h.MovePlayerToChunk(kFarAway);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, kTarget));
    REQUIRE(h.Chunks().GetChunkShared(kTarget) == nullptr);

    // Return. A brand-new Chunk occupies the coord, with its own terrain job.
    h.MovePlayerToChunk(kTarget);
    h.Step(/*pumpJobs=*/false);

    const auto reloaded = h.Chunks().GetChunkShared(kTarget);
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->GetInstanceId() != originalId);   // genuinely a different instance
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, kTarget) >= 1);

    // Run the STALE job (oldest first) and let the manager drain whatever it produced.
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Terrain, kTarget));
    h.Chunks().Update();

    // The new chunk must not have been marked generated on the strength of a result
    // belonging to a previous instance.
    CHECK_FALSE(reloaded->IsGenerated());

    // Its own job then completes normally and produces real terrain.
    REQUIRE(h.Quiesce());
    const std::vector<uint32_t> blocks = h.BlocksOf(kTarget);
    REQUIRE(blocks.size() == static_cast<size_t>(Chunk::VOLUME));
    CHECK(HasVariedContent(blocks));
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

TEST_CASE("streaming leaves no chunk permanently ungenerated") {
    // LoadChunk used to insert into loadedChunks BEFORE checking that a region was
    // available, and returned on failure without undoing the insertion. The chunk then
    // sat there ungenerated forever, and AllNeighborsGenerated blocked meshing for all
    // six of its neighbours for the rest of the session. LoadChunk now rolls back.
    //
    // This originally simulated the condition by dropping a terrain job outright, which
    // no longer corresponds to anything reachable: jobs are cancelled, never silently
    // discarded, and the rollback path handles the real failure. What is worth pinning
    // is the invariant itself - after streaming settles, nothing is left half-loaded.
    WorldTestHarness h({});

    // Move around enough to exercise load, unload and reload paths.
    for (int i = 0; i < 6; ++i) {
        h.MovePlayerToChunk(ChunkCoord{i % 3, 5, (i / 3) % 3});
        h.Step(/*pumpJobs=*/false);
        h.Scheduler().RunAllOfKind(ChunkJobKind::Unload);
    }
    REQUIRE(h.Quiesce());

    const auto stats = h.Chunks().GetStats();
    CAPTURE(stats.loadedChunks);
    CAPTURE(stats.generatedChunks);

    // Every loaded chunk finished generating.
    CHECK(stats.generatedChunks == stats.loadedChunks);

    for (const ChunkCoord coord : h.Chunks().GetLoadedCoords()) {
        const auto chunk = h.Chunks().GetChunkShared(coord);
        REQUIRE(chunk != nullptr);
        CAPTURE(coord.x); CAPTURE(coord.y); CAPTURE(coord.z);
        REQUIRE(chunk->IsGenerated());
    }
}

} // TEST_SUITE("world.tier3.lifecycle")
