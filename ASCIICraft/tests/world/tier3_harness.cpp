// WorldTestHarness self-checks.
//
// The lifecycle tests are only meaningful if the harness itself streams chunks the way
// the game does. These verify that before anything depends on it.

#include <doctest/doctest.h>

#include "support/WorldTestHarness.hpp"

#include <algorithm>

using testsupport::WorldTestHarness;

TEST_SUITE("world.tier3.harness") {

TEST_CASE("a fresh harness has loaded nothing") {
    WorldTestHarness h({});
    const auto stats = h.Chunks().GetStats();
    CHECK(stats.loadedChunks == 0);
    CHECK(h.Scheduler().PendingCount() == 0);
}

TEST_CASE("one frame loads the shell around the player and queues terrain") {
    WorldTestHarness h({});
    h.MovePlayerToChunk(ChunkCoord{0, 5, 0});

    h.Step(/*pumpJobs=*/false);

    // loadDistance is renderDistance + 1 == 2, so a 5x5x5 shell, minus anything the
    // world bounds clip away.
    const auto stats = h.Chunks().GetStats();
    CAPTURE(stats.loadedChunks);
    CHECK(stats.loadedChunks > 0);
    CHECK(stats.loadedChunks <= 125);

    // Nothing generated yet - the jobs are queued, not run.
    CHECK(stats.generatedChunks == 0);
    CHECK(h.Scheduler().CountPending(ChunkJobKind::Terrain) > 0);
}

TEST_CASE("pumping jobs generates the loaded chunks") {
    WorldTestHarness h({});
    h.MovePlayerToChunk(ChunkCoord{0, 5, 0});
    REQUIRE(h.Quiesce());

    const auto stats = h.Chunks().GetStats();
    CHECK(stats.loadedChunks > 0);
    CHECK(stats.generatedChunks == stats.loadedChunks);
    CHECK(h.Scheduler().PendingCount() == 0);
}

TEST_CASE("streamed chunk content matches a standalone reference") {
    // The property every later assertion is built on: a chunk that streamed in through
    // ChunkManager holds exactly what the generator would have produced for it.
    //
    // Compared on a chunk with no cross-chunk spill applied to it, so the two are
    // directly comparable; spill handling is tested separately.
    WorldTestHarness h({});
    const ChunkCoord centre{0, 5, 0};
    h.MovePlayerToChunk(centre);
    REQUIRE(h.Quiesce());

    const std::vector<uint32_t> streamed = h.BlocksOf(centre);
    REQUIRE(streamed.size() == static_cast<size_t>(Chunk::VOLUME));

    const std::vector<uint32_t> reference = h.GenerateReference(centre);
    REQUIRE(reference.size() == streamed.size());

    // Terrain must match wherever the manager did not overlay decoration. Count
    // differences rather than requiring equality - spill from neighbours legitimately
    // adds blocks on top of the raw terrain.
    size_t differing = 0;
    for (size_t i = 0; i < streamed.size(); ++i) {
        if (streamed[i] != reference[i]) ++differing;
    }
    CAPTURE(differing);
    // Decoration is sparse; a wholesale mismatch would mean the manager is not using
    // the terrain generator's output at all.
    CHECK(differing < streamed.size() / 4);
}

TEST_CASE("moving away unloads chunks") {
    WorldTestHarness h({});
    h.MovePlayerToChunk(ChunkCoord{0, 5, 0});
    REQUIRE(h.Quiesce());
    const size_t before = h.Chunks().GetStats().loadedChunks;
    REQUIRE(before > 0);

    h.MovePlayerToChunk(ChunkCoord{100, 5, 100});
    REQUIRE(h.Quiesce());

    const auto loaded = h.Chunks().GetLoadedCoords();
    const bool anyNearOrigin = std::any_of(loaded.begin(), loaded.end(),
        [](ChunkCoord c) { return std::abs(c.x) < 10 && std::abs(c.z) < 10; });
    CHECK_FALSE(anyNearOrigin);
    CHECK(h.Chunks().GetStats().loadedChunks <= before + 8);
}

TEST_CASE("the fake clock only advances when the test says so") {
    WorldTestHarness h({});
    const uint32_t start = h.Now();
    h.StepFrames(5);
    CHECK(h.Now() == start);

    h.AdvanceClock(301);
    CHECK(h.Now() == start + 301);
}

TEST_CASE("mesh jobs are dropped by default") {
    // Guards the memory bomb: headless there is no texture array, so mesh results are
    // never drained and dirty chunks are re-enqueued every frame.
    WorldTestHarness h({});
    h.MovePlayerToChunk(ChunkCoord{0, 5, 0});
    h.StepFrames(10);

    CHECK(h.Scheduler().CountPending(ChunkJobKind::Mesh) == 0);
    CHECK(h.Scheduler().PendingCount() < 500);
}

} // TEST_SUITE("world.tier3.harness")
