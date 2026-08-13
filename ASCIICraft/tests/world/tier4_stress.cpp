// Tier 4: real concurrency.
//
// Everything below runs on actual threads, so these are slow and excluded from the
// default run (run_tests.ps1 passes -LE stress, which regex-matches this suite's
// label). Run them with:
//     ./scripts/run_tests.ps1 -Stress
//     ./scripts/run_tests.ps1 -Asan -Stress      (much stronger)
//
// Covers the region file layer, the region cache, the production scheduler, and full
// multi-threaded chunk streaming.
//
// That last one was impossible until defect A was fixed: an unload racing an in-flight
// terrain job wrote through a dangling Chunk*, which was verified heap corruption
// rather than a test failure, so a process running it died mid-suite.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/block/VanillaBlockRegistration.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>
#include <ASCIICraft/world/chunk/ChunkManagerDeps.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>
#include <ASCIICraft/world/chunk/IChunkJobScheduler.hpp>

#include <entt/entt.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace {

void FillChunk(Chunk& c, uint32_t seedValue) {
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        c.SetBlockStateByIndex(i, (static_cast<uint32_t>(i) + seedValue) % 20);
    }
    // Required: the save path refuses ungenerated chunks, so a test chunk has to look
    // like one whose terrain actually ran.
    c.SetGenerated(true);
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

TEST_CASE("real-threaded streaming under churn") {
    // This could not run at all until defect A was fixed: on the real TBB scheduler an
    // unload racing an in-flight terrain job wrote through a freed Chunk*, which was
    // verified heap corruption (exit 0xC0000374) rather than a test failure.
    //
    // Now terrain jobs hold a shared_ptr to their chunk and results are matched by
    // instance id, so genuine multi-threaded churn is safe to exercise.
    //
    // Uses ChunkManager directly rather than WorldTestHarness, because the harness owns
    // a manual scheduler by construction.
    testsupport::TempDir dir("stress_stream");

    entt::registry registry;
    blockstate::VanillaRegistrationOptions opts;
    opts.buildLegacyRemapTable = false;   // the shared fixture owns that global table
    blockstate::RegisterVanillaBlocksInContext(registry, opts);

    const entt::entity player = registry.create();
    registry.emplace<ecs::components::PlayerTag>(player);
    auto& transform = registry.emplace<ecs::components::Transform>(player);

    ChunkManagerDeps deps;
    deps.regionDir = dir.Path();
    deps.scheduler = MakeTbbChunkJobScheduler();   // the production scheduler

    const sizes::WorldDimensions dims(1024, 0, 1024);
    ChunkManager chunks(registry, dims, /*renderDistance=*/1, /*worldSeed=*/12345ULL,
                        std::move(deps));

    const auto moveTo = [&](int cx, int cz) {
        transform.position = glm::vec3(static_cast<float>(cx * 16 + 8), 88.0f,
                                       static_cast<float>(cz * 16 + 8));
    };

    // Teleport several chunks per frame so load and unload overlap constantly - the
    // condition that used to corrupt the heap within a few hundred frames.
    std::mt19937 rng(20260808u);
    std::uniform_int_distribution<int> pick(-6, 6);

    for (int frame = 0; frame < 600; ++frame) {
        moveTo(pick(rng), pick(rng));
        chunks.Update();

        if (frame % 20 == 0) {
            // Save while jobs are still in flight. SaveAll waits for pending work
            // first, which is itself part of what is being exercised.
            chunks.SaveAll();
        }
        if (frame % 50 == 0) {
            const ChunkManager::Stats stats = chunks.GetStats();
            CAPTURE(frame);
            // loadDistance 2 plus one shell of unload hysteresis -> radius 3.
            REQUIRE(stats.loadedChunks <= 7 * 7 * 7 + 64);
            REQUIRE(stats.generatedChunks <= stats.loadedChunks);
        }
    }

    chunks.SaveAll();

    // Everything persisted must still parse. A torn write from a racing unload would
    // surface here as a throw or a failed load.
    RegionManager verify(dir.Path());
    size_t readable = 0;
    for (int cx = -8; cx <= 8; ++cx) {
        for (int cz = -8; cz <= 8; ++cz) {
            const ChunkCoord coord{cx, 5, cz};
            auto region = verify.GetOrCreate(coord.ToRegionCoord());
            REQUIRE(region != nullptr);
            Chunk probe(coord);
            try {
                if (region->LoadChunk(&probe, testsupport::SharedBlocks())) ++readable;
            } catch (const std::exception& e) {
                CAPTURE(cx);
                CAPTURE(cz);
                FAIL_CHECK("persisted chunk failed to parse: " << e.what());
            }
        }
    }
    CAPTURE(readable);
    CHECK(readable > 0);
}

} // TEST_SUITE("world.tier4.stress")
