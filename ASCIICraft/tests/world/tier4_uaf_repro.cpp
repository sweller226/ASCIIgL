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
// MEASURED behaviour while the defect exists:
//
//   FastDebug (no sanitizer)  process death, exit 0xC0000374 STATUS_HEAP_CORRUPTION.
//                             Reliable. This is the reproducer that works.
//
//   ASan build                NOT detected. The runtime is confirmed active (the exe
//                             imports clang_rt.asan_dynamic and the __asan_*
//                             interceptors resolve), and the stale job is confirmed to
//                             run, yet no heap-use-after-free is reported. Retried
//                             with quarantine_size_mb=512 and max_redzone=2048 to rule
//                             out the ~16 KiB Chunk allocation bypassing the
//                             quarantine; still silent. Root cause not established -
//                             most likely a gap in MSVC ASan's coverage of this
//                             allocation path rather than anything about the defect.
//
// Do not rely on ASan to confirm a fix for defect A. Use the FastDebug crash, plus the
// deterministic pins in tier3_lifecycle.cpp which assert the precondition
// (chunk destroyed while its terrain job is still queued) without invoking the UB.
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

    // No allocation churn here, deliberately.
    //
    // Churning the heap first causes the freed 16 KiB block to be RECYCLED, and the
    // stale write then lands in live memory belonging to another object. That is
    // silent corruption: ASan sees a write to a valid allocation and says nothing,
    // and the damage surfaces later as unrelated garbage. Verified - with churn in
    // place this test reached the end cleanly under ASan.
    //
    // Leaving the block in ASan's quarantine instead means the write hits poisoned
    // memory and produces the diagnostic naming both the free and the write.
    //
    // Worth understanding both halves: the quarantined case is how you DEBUG this,
    // the recycled case is how it actually BEHAVES in the running game, and is why it
    // presented as "one chunk turned to dirt" long after the frame that caused it.

    // The stale job must still be queued - that IS the defect. If this fails, the
    // job was cancelled somewhere and there is nothing left to reproduce.
    REQUIRE(h.Scheduler().CountPending(ChunkJobKind::Terrain, target) == 1);

    // The write-after-free. Expected outcomes:
    //   ASan build      -> heap-use-after-free report, process aborts
    //   FastDebug build -> heap corruption, exit 0xC0000374
    REQUIRE(h.Scheduler().RunFirst(ChunkJobKind::Terrain, target));

    // Only reached once the defect is fixed - at which point the job must have been
    // cancelled and nothing written.
    FAIL_CHECK("expected the stale terrain job to be cancelled or the process to die");
}

} // TEST_SUITE("world.tier4.stress")
