// Region file behaviour: capacity, reopen, growth, and the batch-save lock.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

void FillChunk(Chunk& c, uint32_t seedValue) {
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        c.SetBlockStateByIndex(i, (static_cast<uint32_t>(i) + seedValue) % 20);
    }
}

uintmax_t FileSize(const fs::path& p) {
    std::error_code ec;
    const uintmax_t s = fs::file_size(p, ec);
    return ec ? 0 : s;
}

} // namespace

TEST_SUITE("world.tier2.regionfile") {

TEST_CASE("many chunks in one region survive a close and reopen") {
    testsupport::TempDir dir("rf_many");
    constexpr int kCount = 100;

    {
        RegionFile rf(RegionCoord{0, 0, 0}, dir.Path());
        REQUIRE(rf.BeginBatchSave());
        for (int i = 0; i < kCount; ++i) {
            Chunk c(ChunkCoord{i % 32, (i / 32) % 32, i / 1024});
            FillChunk(c, static_cast<uint32_t>(i));
            rf.SaveChunkInBatch(&c, testsupport::SharedBlocks());
        }
        rf.EndBatchSave();
    }

    RegionFile rf(RegionCoord{0, 0, 0}, dir.Path());
    for (int i = 0; i < kCount; ++i) {
        const ChunkCoord coord{i % 32, (i / 32) % 32, i / 1024};
        Chunk expected(coord);
        FillChunk(expected, static_cast<uint32_t>(i));

        Chunk loaded(coord);
        CAPTURE(i);
        REQUIRE(rf.LoadChunk(&loaded, testsupport::SharedBlocks()));
        for (int b = 0; b < Chunk::VOLUME; ++b) {
            REQUIRE(loaded.GetBlockStateByIndex(b) == expected.GetBlockStateByIndex(b));
        }
    }
}

TEST_CASE("the region path encodes its coordinate") {
    testsupport::TempDir dir("rf_path");
    RegionFile rf(RegionCoord{-3, 2, -7}, dir.Path());
    CHECK(rf.GetRegionCoord() == RegionCoord{-3, 2, -7});
    CHECK(fs::path(rf.GetPath()).filename() == "r_-3.2.-7");
}

TEST_CASE("a region file is not opened until first use") {
    // RegionFile is constructed eagerly by RegionManager::GetOrCreate but should not
    // touch the disk until something reads or writes.
    testsupport::TempDir dir("rf_lazy");
    RegionFile rf(RegionCoord{0, 0, 0}, dir.Path());
    CHECK_FALSE(rf.IsFileOpen());
    CHECK_FALSE(fs::exists(dir / "r_0.0.0"));
}

TEST_CASE("RegionManager caches by coordinate and honours its directory") {
    testsupport::TempDir dir("rf_mgr");
    RegionManager manager(dir.Path());

    auto a = manager.GetOrCreate(RegionCoord{0, 0, 0});
    auto again = manager.GetOrCreate(RegionCoord{0, 0, 0});
    auto other = manager.GetOrCreate(RegionCoord{1, 0, 0});

    REQUIRE(a);
    CHECK(a == again);
    CHECK(a != other);
    CHECK(fs::path(a->GetPath()).parent_path() == dir.Path());
}

TEST_CASE("RegionManager keeps its cache bounded when files are closed") {
    // MAX_REGIONS is 32. Eviction only happens for regions whose file is not open, so
    // this exercises the path that can actually evict.
    testsupport::TempDir dir("rf_lru");
    RegionManager manager(dir.Path());

    for (int i = 0; i < 64; ++i) {
        auto r = manager.GetOrCreate(RegionCoord{i, 0, 0});
        REQUIRE(r);
        // Released immediately, and never opened, so it is evictable.
    }
    // No public size accessor; reaching here without exhausting handles or throwing is
    // the assertion. A leak would surface as file-handle exhaustion at larger counts.
    CHECK(true);
}

// --- DEFECT G: append-only growth --------------------------------------------

TEST_CASE("re-saving an unchanged chunk does not grow the file"
          * doctest::should_fail()) {
    // Blobs are appended and the index repointed; the previous blob is orphaned and
    // never reclaimed. Combined with a 60s autosave that rewrites every loaded chunk
    // regardless of modification, region files grow without bound during a long
    // session. The fix needs a dirty-since-save flag plus blob reuse or compaction.
    testsupport::TempDir dir("rf_growth");
    const ChunkCoord coord{0, 0, 0};
    const fs::path file = dir / "r_0.0.0";

    Chunk c(coord);
    FillChunk(c, 7u);

    {
        RegionFile rf(coord.ToRegionCoord(), dir.Path());
        REQUIRE(rf.SaveChunk(&c, testsupport::SharedBlocks()));
    }
    const uintmax_t afterFirst = FileSize(file);
    REQUIRE(afterFirst > 0);

    for (int i = 0; i < 5; ++i) {
        RegionFile rf(coord.ToRegionCoord(), dir.Path());
        REQUIRE(rf.SaveChunk(&c, testsupport::SharedBlocks()));
    }
    const uintmax_t afterRepeats = FileSize(file);

    CAPTURE(afterFirst);
    CAPTURE(afterRepeats);
    CHECK(afterRepeats == afterFirst);
}

// --- DEFECT G: BeginBatchSave leaks its lock on failure ----------------------

TEST_CASE("a failed BeginBatchSave does not leave the region locked"
          * doctest::should_fail()) {
    // BeginBatchSave takes _batchLock BEFORE calling EnsureOpen. If the open fails it
    // returns false with the lock still held, and every later operation on that
    // RegionFile blocks forever.
    //
    // SaveAll does `if (!region || !region->BeginBatchSave()) continue;` - so one
    // unwritable region silently wedges the save path for the rest of the session.
    //
    // Everything the probe thread can touch is heap-allocated and never freed.
    //
    // Three traps here, each of which turns a red test into a hung suite:
    //   - std::async is unusable: ~future joins, moving the hang to process exit.
    //   - The RegionFile cannot live on the stack. If the probe is parked inside it,
    //     ~RegionFile blocks on the same held mutex, hanging the TEST thread.
    //   - The TempDir must not try to delete a file a parked thread still holds.
    auto* dir = new testsupport::TempDir("rf_lock");
    dir->Keep();

    // Occupy the region's path with a directory so opening it as a file must fail.
    fs::create_directories(dir->Path() / "r_0.0.0");

    auto* rf = new RegionFile(RegionCoord{0, 0, 0}, dir->Path());
    const bool began = rf->BeginBatchSave();
    REQUIRE_FALSE(began);   // precondition: the open really did fail

    auto* finished = new std::atomic<bool>{false};

    std::thread([rf, finished]() {
        Chunk c(ChunkCoord{0, 0, 0});
        try {
            (void)rf->LoadChunk(&c, testsupport::SharedBlocks());
        } catch (const std::exception&) {
            // Throwing is fine; blocking forever is not.
        }
        finished->store(true, std::memory_order_release);
    }).detach();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!finished->load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(finished->load(std::memory_order_acquire));
}

} // TEST_SUITE("world.tier2.regionfile")
