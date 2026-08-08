// DEFECT A: on-demand heap corruption reproducer.
//
// This test CRASHES THE PROCESS when the defect is present. It is skipped by default
// and carries the `stress` label so `ctest` never runs it; run_tests.ps1 passes
// -LE stress unless you ask for it.
//
// Run it deliberately:
//     ASCIICraft_tests.exe --test-case="*heap corruption*" --no-skip
//     ./scripts/run_tests.ps1 -Stress -Filter "heap corruption"
//
// Expected while the defect exists: process death, exit 0xC0000374
// (STATUS_HEAP_CORRUPTION). Under /fsanitize=address you instead get a clean
// heap-use-after-free report naming both the free site (ChunkManager::UnloadChunk)
// and the write site (TerrainGenerator::GenerateChunkInto), which is the fastest way
// to confirm a fix.
//
// Mechanism: EnqueueTerrainGen captures a raw Chunk*. UnloadChunk erases the
// shared_ptr and hands the last reference to an unload job in the same queue, with no
// cancellation and no ordering. Run the unload first and the terrain job then writes
// Chunk::VOLUME state ids into freed memory. For an underground chunk every one of
// those is dirt - which is exactly the "one chunk got corrupted and entirely filled
// with dirt" report.

#include <doctest/doctest.h>

#include "support/WorldTestHarness.hpp"

#include <memory>

using testsupport::WorldTestHarness;

TEST_SUITE("world.tier4.stress") {

TEST_CASE("DEFECT A: stale terrain job writes into a freed chunk (heap corruption)"
          * doctest::skip()) {
    WorldTestHarness h({});
    const ChunkCoord target{0, 5, 0};

    h.MovePlayerToChunk(target);
    h.Step(/*pumpJobs=*/false);
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, target) == 1);

    std::weak_ptr<const Chunk> watch = h.Chunks().GetChunkShared(target);
    REQUIRE_FALSE(watch.expired());

    // Walk away so the chunk is unloaded while its terrain job is still queued.
    h.MovePlayerToChunk(ChunkCoord{200, 5, 200});
    h.Step(/*pumpJobs=*/false);

    // Force the ordering the real queue permits but does not guarantee.
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Unload, target));
    REQUIRE(watch.expired());   // the Chunk is gone

    // Allocate churn so the freed 16 KiB block is likely recycled, making the write
    // land on live data rather than an untouched free block.
    std::vector<std::unique_ptr<std::vector<uint32_t>>> churn;
    for (int i = 0; i < 64; ++i) {
        churn.push_back(std::make_unique<std::vector<uint32_t>>(Chunk::VOLUME, 0xABCDEF01u));
    }

    // The write-after-free. Process death is expected here.
    h.Scheduler().RunFirst(ChunkJobKind::Terrain, target);

    // Only reached once the defect is fixed - at which point the job must have been
    // cancelled and nothing should have been written.
    FAIL_CHECK("expected the stale terrain job to be cancelled or the process to die");
}

} // TEST_SUITE("world.tier4.stress")
