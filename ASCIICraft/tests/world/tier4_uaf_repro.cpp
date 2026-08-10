// DEFECT A regression pin: a terrain job must never write into a freed chunk.
//
// HISTORY. EnqueueTerrainGen used to capture a raw Chunk*. UnloadChunk erased the
// shared_ptr and handed the last reference to an unload job in the same unordered
// queue, with no cancellation and no ordering guarantee. Running the unload first left
// the terrain job writing Chunk::VOLUME state ids through a dangling pointer - for an
// underground chunk, 4096 dirt blocks into whatever now owned that allocation. That is
// the "one chunk got corrupted and entirely filled with dirt" report, and it
// reproduced here as process death with exit 0xC0000374 (STATUS_HEAP_CORRUPTION).
//
// This file used to be skipped because it killed the test process. It now runs in the
// normal suite: the sequence below must complete safely.
//
// Worth recording, because it explains why the bug was so hard to find: when the heap
// had churned enough for the freed 16 KiB block to be RECYCLED, the stale write landed
// in live memory belonging to another object. No crash, no sanitizer report - just
// silent corruption surfacing much later as unrelated garbage. ASan never flagged this
// even with quarantine_size_mb=512.

#include <doctest/doctest.h>

#include "support/WorldTestHarness.hpp"

#include <memory>
#include <vector>

using testsupport::WorldTestHarness;

TEST_SUITE("world.tier3.lifecycle") {

TEST_CASE("a stale terrain job cannot write into a freed chunk") {
    WorldTestHarness h({});
    const ChunkCoord target{0, 5, 0};

    h.MovePlayerToChunk(target);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, target) == 1);

    std::weak_ptr<const Chunk> watch = h.Chunks().GetChunkShared(target);
    REQUIRE_FALSE(watch.expired());

    // Walk away so the chunk unloads with its terrain job still queued.
    h.MovePlayerToChunk(ChunkCoord{200, 5, 200});
    h.Step(/*pumpJobs=*/false);

    // Force the ordering that used to corrupt the heap: unload BEFORE terrain.
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, target));

    // The chunk is out of loadedChunks but still alive - the queued job holds a
    // reference. This is the property that makes the next step safe.
    CHECK(h.Chunks().GetChunkShared(target) == nullptr);
    CHECK_FALSE(watch.expired());

    // Heap churn, to recycle the block if it had been freed. Under the old code this
    // is what turned the crash into silent corruption.
    std::vector<std::unique_ptr<std::vector<uint32_t>>> churn;
    for (int i = 0; i < 64; ++i) {
        churn.push_back(std::make_unique<std::vector<uint32_t>>(Chunk::VOLUME, 0xABCDEF01u));
    }

    // The write that used to be a use-after-free. Must now be safe: the job sees the
    // cancelled flag and returns without generating.
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Terrain, target));

    // Nothing was resurrected, and no result was applied to the coord.
    h.Chunks().Update();
    CHECK(h.Chunks().GetChunkShared(target) == nullptr);

    // The churn buffers are untouched - a stale write would have scribbled over one.
    for (const auto& buffer : churn) {
        REQUIRE((*buffer)[0] == 0xABCDEF01u);
        REQUIRE((*buffer)[Chunk::VOLUME - 1] == 0xABCDEF01u);
    }

    // Once the job releases its reference the chunk is reclaimed - no leak.
    h.Scheduler().RunAll();
    churn.clear();
    CHECK(watch.expired());
}

} // TEST_SUITE("world.tier3.lifecycle")
