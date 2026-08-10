// Tier 4: real concurrency.
//
// Everything below runs on actual threads, so these are slow and excluded from the
// default run (run_tests.ps1 passes -LE stress, which regex-matches this suite's
// label). Run them with:
//     ./scripts/run_tests.ps1 -Stress
//     ./scripts/run_tests.ps1 -Asan -Stress      (much stronger)
//
// Scope note. The obvious Tier 4 test - drive real chunk streaming on the TBB
// scheduler under churn - CANNOT run while defect A stands: an unload racing an
// in-flight terrain job writes through a dangling Chunk*, which is verified heap
// corruption, not a hypothesis. A process that dies mid-suite reports nothing useful,
// so that scenario lives in tier4_uaf_repro.cpp behind an explicit skip.
//
// What remains here is genuinely concurrent and genuinely safe to run: the region
// file layer and the region cache, neither of which touches chunk lifetime.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>
#include <ASCIICraft/world/chunk/IChunkJobScheduler.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

void FillChunk(Chunk& c, uint32_t seedValue) {
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        c.SetBlockStateByIndex(i, (static_cast<uint32_t>(i) + seedValue) % 20);
    }
}

} // namespace

TEST_SUITE("world.tier4.stress") {

TEST_CASE("concurrent readers and writers on one RegionFile") {
    // RegionFile guards itself with _mutex and is used from both the main thread
    // (SaveAll, LoadChunk) and unload worker tasks. This is the contention that
    // actually happens in the game.
    testsupport::TempDir dir("stress_region");
    auto region = std::make_shared<RegionFile>(RegionCoord{0, 0, 0}, dir.Path());

    // Seed some content so readers have something to find.
    {
        REQUIRE(region->BeginBatchSave());
        for (int i = 0; i < 16; ++i) {
            Chunk c(ChunkCoord{i, 0, 0});
            FillChunk(c, static_cast<uint32_t>(i));
            region->SaveChunkInBatch(&c, testsupport::SharedBlocks());
        }
        region->EndBatchSave();
    }

    std::atomic<int> failures{0};
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 40 && !stop.load(std::memory_order_relaxed); ++i) {
                const ChunkCoord coord{(i + t) % 16, 0, 0};
                try {
                    Chunk c(coord);
                    (void)region->LoadChunk(&c, testsupport::SharedBlocks());
                    MetaBucket m;
                    (void)region->LoadMetaData(coord, &m, testsupport::SharedBlocks());
                } catch (const std::exception&) {
                    // A read losing a race with a concurrent append is acceptable;
                    // a crash or a hang is not.
                }
            }
        });
    }
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 20 && !stop.load(std::memory_order_relaxed); ++i) {
                const ChunkCoord coord{(i * 2 + t) % 16, 0, 0};
                try {
                    Chunk c(coord);
                    FillChunk(c, static_cast<uint32_t>(i + 100));
                    region->SaveChunkForUnload(&c, coord, nullptr, false,
                                               testsupport::SharedBlocks());
                } catch (const std::exception&) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    CAPTURE(failures.load());
    CHECK(failures.load() == 0);

    // The file must still be readable afterwards - a torn write would surface here.
    RegionFile reopened(RegionCoord{0, 0, 0}, dir.Path());
    int readable = 0;
    for (int i = 0; i < 16; ++i) {
        Chunk c(ChunkCoord{i, 0, 0});
        try {
            if (reopened.LoadChunk(&c, testsupport::SharedBlocks())) ++readable;
        } catch (const std::exception&) {
        }
    }
    CAPTURE(readable);
    CHECK(readable > 0);
}

TEST_CASE("concurrent GetOrCreate on RegionManager") {
    // RegionManager::GetOrCreate is called from the main thread only today, but it
    // hands out shared_ptrs that outlive the call and it maintains an LRU list under
    // its own mutex. This pins that the locking is real.
    testsupport::TempDir dir("stress_mgr");
    RegionManager manager(dir.Path());

    std::atomic<int> nulls{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 200; ++i) {
                auto r = manager.GetOrCreate(RegionCoord{(i + t) % 40, 0, 0});
                if (!r) nulls.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) th.join();

    CHECK(nulls.load() == 0);
}

TEST_CASE("the region cache stays bounded under heavy churn") {
    // MAX_REGIONS is 32 and eviction skips regions whose file is open. Nothing here
    // opens a file, so the cache must actually shrink rather than growing to 10000.
    testsupport::TempDir dir("stress_lru");
    RegionManager manager(dir.Path());

    for (int i = 0; i < 10000; ++i) {
        auto r = manager.GetOrCreate(RegionCoord{i % 500, 0, 0});
        REQUIRE(r != nullptr);
    }
    // No size accessor; surviving without exhausting handles or memory is the
    // assertion. Under ASan a leak here also shows up at exit.
    CHECK(true);
}

TEST_CASE("the TBB scheduler handles sustained submission") {
    // Exercises the production scheduler directly, without chunk lifetime in play.
    auto scheduler = MakeTbbChunkJobScheduler();
    std::atomic<int> ran{0};

    for (int wave = 0; wave < 20; ++wave) {
        for (int i = 0; i < 200; ++i) {
            scheduler->Run(ChunkJobTag{ChunkJobKind::Terrain, ChunkCoord{i, 0, wave}},
                           [&ran]() { ran.fetch_add(1, std::memory_order_relaxed); });
        }
        scheduler->Wait();
    }

    CHECK(ran.load() == 20 * 200);
}

TEST_CASE("DEFECT A: real-threaded streaming under churn"
          * doctest::skip()) {
    // Deliberately not runnable. With the real TBB scheduler, an unload racing an
    // in-flight terrain job writes Chunk::VOLUME state ids through a freed pointer.
    // That is confirmed heap corruption (exit 0xC0000374), so this would kill the
    // process rather than report anything.
    //
    // Once defect A is fixed, delete the skip decorator and implement the body:
    //   - WorldTestHarness with MakeTbbChunkJobScheduler instead of the manual one
    //   - 2000 frames of 3-chunk-per-frame teleports
    //   - every 50 frames assert the column profile invariant and bounded loadedChunks
    //   - SaveAll every 20 frames while jobs are in flight, then verify from a second
    //     ChunkManager that every persisted chunk still parses and matches reference
    //
    // Until then, tier3_integrity covers the same ground deterministically and
    // tier4_uaf_repro reproduces the crash on demand.
    FAIL_CHECK("not implemented until defect A is fixed - see comment");
}

} // TEST_SUITE("world.tier4.stress")
