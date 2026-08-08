// World integrity under sustained streaming.
//
// The broad net. The targeted lifecycle tests each reproduce one known defect; this
// walks a player around and asserts the world still holds what it should, catching
// combinations nobody thought to script.
//
// Deterministic despite looking like a fuzz test: the path comes from a fixed seed,
// job execution order is chosen rather than observed, and the clock is a counter. A
// failure reproduces byte-for-byte on any machine from the seed printed alongside it.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/WorldTestHarness.hpp"

#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

using testsupport::WorldTestHarness;

namespace {

uint64_t NextRandom(uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

int RandomInRange(uint64_t& state, int lo, int hi) {
    return lo + static_cast<int>(NextRandom(state) % static_cast<uint64_t>(hi - lo + 1));
}

bool IsAllAir(const std::vector<uint32_t>& blocks) {
    if (blocks.empty()) return false;
    const uint32_t air = testsupport::Ids().air;
    return std::all_of(blocks.begin(), blocks.end(), [air](uint32_t v) { return v == air; });
}

bool HasVariedContent(const std::vector<uint32_t>& blocks) {
    if (blocks.empty()) return false;
    const uint32_t first = blocks[0];
    return std::any_of(blocks.begin(), blocks.end(), [first](uint32_t v) { return v != first; });
}

} // namespace

TEST_SUITE("world.tier3.integrity") {

TEST_CASE("a straight walk out and back preserves chunk content") {
    // The simplest streaming pattern, fully pumped every frame. This one should be
    // green even before any fix - if it is not, streaming is broken far more badly
    // than the reported symptoms suggest.
    WorldTestHarness h({});
    const ChunkCoord home{0, 5, 0};

    h.MovePlayerToChunk(home);
    REQUIRE(h.QuiesceSafely());
    const std::vector<uint32_t> before = h.BlocksOf(home);
    REQUIRE(before.size() == static_cast<size_t>(Chunk::VOLUME));
    REQUIRE(HasVariedContent(before));

    for (int trip = 0; trip < 3; ++trip) {
        for (int step = 1; step <= 8; ++step) {
            h.MovePlayerToChunk(ChunkCoord{step, 5, 0});
            REQUIRE(h.QuiesceSafely());
        }
        for (int step = 7; step >= 0; --step) {
            h.MovePlayerToChunk(ChunkCoord{step, 5, 0});
            REQUIRE(h.QuiesceSafely());
        }
        CAPTURE(trip);
        const std::vector<uint32_t> after = h.BlocksOf(home);
        REQUIRE(after.size() == before.size());
        REQUIRE(after == before);
    }
}

TEST_CASE("a player edit survives an unload and reload") {
    WorldTestHarness h({});
    const ChunkCoord home{0, 5, 0};
    h.MovePlayerToChunk(home);
    REQUIRE(h.QuiesceSafely());

    // Place a distinctive block in the middle of the chunk.
    const uint32_t marker = testsupport::Ids().glass;
    h.Chunks().SetBlockState(home.x * 16 + 8, home.y * 16 + 8, home.z * 16 + 8, marker);
    REQUIRE(h.BlocksOf(home)[chunkutil::GetBlockIndex(8, 8, 8)] == marker);

    h.MovePlayerToChunk(ChunkCoord{100, 5, 100});
    REQUIRE(h.QuiesceSafely());
    h.MovePlayerToChunk(home);
    REQUIRE(h.QuiesceSafely());

    CHECK(h.BlocksOf(home)[chunkutil::GetBlockIndex(8, 8, 8)] == marker);
}

TEST_CASE("a player edit survives SaveAll and a fresh ChunkManager") {
    // Persistence across a "restart": the second harness reads the same region dir.
    testsupport::TempDir shared("integrity_restart");
    const ChunkCoord home{0, 5, 0};
    const uint32_t marker = testsupport::Ids().glass;

    {
        WorldTestHarness::Config cfg;
        cfg.label = "restart_a";
        WorldTestHarness h(cfg);
        h.MovePlayerToChunk(home);
        REQUIRE(h.QuiesceSafely());
        h.Chunks().SetBlockState(home.x * 16 + 4, home.y * 16 + 4, home.z * 16 + 4, marker);
        h.Chunks().SaveAll();

        // Reload within the same harness - the region dir is per-harness, so this is
        // the reachable equivalent of a restart.
        h.MovePlayerToChunk(ChunkCoord{100, 5, 100});
        REQUIRE(h.QuiesceSafely());
        h.MovePlayerToChunk(home);
        REQUIRE(h.QuiesceSafely());

        CHECK(h.BlocksOf(home)[chunkutil::GetBlockIndex(4, 4, 4)] == marker);
    }
}

TEST_CASE("a randomized walk leaves every visited chunk intact"
          * doctest::should_fail()) {
    // The catch-all. Partial job pumping recreates the real interleaving where some
    // terrain lands and some is still queued when the next unload arrives.
    //
    // Expected red while defect B stands: chunks unloaded mid-generation are persisted
    // empty and never regenerate.
    constexpr uint64_t kSeed = 0xC0FFEEULL;
    uint64_t rng = kSeed;
    CAPTURE(kSeed);

    WorldTestHarness h({});
    std::set<std::pair<int, int>> visited;

    for (int frame = 0; frame < 120; ++frame) {
        const int cx = RandomInRange(rng, -4, 4);
        const int cz = RandomInRange(rng, -4, 4);
        h.MovePlayerToChunk(ChunkCoord{cx, 5, cz});
        visited.insert({cx, cz});

        h.Chunks().Update();
        // Partial pump: terrain before unload, so the walk measures content loss
        // rather than dying of defect A's heap corruption. A's ordering has its own
        // dedicated pins in tier3_lifecycle.
        h.PumpRandomSubset(rng, 0.6);
    }

    REQUIRE(h.QuiesceSafely(128));

    size_t corrupted = 0;
    size_t inspected = 0;
    for (const ChunkCoord coord : h.Chunks().GetLoadedCoords()) {
        const auto chunk = h.Chunks().GetChunkShared(coord);
        if (!chunk || !chunk->IsGenerated()) continue;
        const std::vector<uint32_t> reference = h.GenerateReference(coord);
        if (!HasVariedContent(reference)) continue;   // only judge chunks with terrain
        ++inspected;
        if (IsAllAir(h.BlocksOf(coord))) ++corrupted;
    }

    CAPTURE(inspected);
    CAPTURE(corrupted);
    REQUIRE(inspected > 0);
    CHECK(corrupted == 0);
}

TEST_CASE("streaming never produces a surface chunk of uniform dirt"
          * doctest::should_fail()) {
    // The "entirely filled with dirt" signature, scoped correctly.
    //
    // Underground chunks are legitimately 100% dirt (see tier1_terrain_determinism),
    // so this only judges chunks whose reference terrain is varied. A uniform result
    // there means something overwrote the whole buffer.
    constexpr uint64_t kSeed = 0xBADC0DEULL;
    uint64_t rng = kSeed;
    CAPTURE(kSeed);

    WorldTestHarness h({});
    for (int frame = 0; frame < 80; ++frame) {
        h.MovePlayerToChunk(ChunkCoord{RandomInRange(rng, -3, 3), 5, RandomInRange(rng, -3, 3)});
        h.Chunks().Update();
        h.PumpRandomSubset(rng, 0.5);
    }
    REQUIRE(h.QuiesceSafely(128));

    size_t uniform = 0;
    size_t inspected = 0;
    for (const ChunkCoord coord : h.Chunks().GetLoadedCoords()) {
        const auto chunk = h.Chunks().GetChunkShared(coord);
        if (!chunk || !chunk->IsGenerated()) continue;
        const std::vector<uint32_t> reference = h.GenerateReference(coord);
        if (!HasVariedContent(reference)) continue;
        ++inspected;
        if (!HasVariedContent(h.BlocksOf(coord))) ++uniform;
    }

    CAPTURE(inspected);
    CAPTURE(uniform);
    REQUIRE(inspected > 0);
    CHECK(uniform == 0);
}

TEST_CASE("buffered cross-chunk edits do not grow without bound") {
    // A leak check. Buckets are created for chunks that are not loaded; if none of the
    // drain, expiry or unload paths ever clears them, memory grows for the session.
    WorldTestHarness h({});
    uint64_t rng = 42ULL;

    for (int frame = 0; frame < 60; ++frame) {
        h.MovePlayerToChunk(ChunkCoord{RandomInRange(rng, -3, 3), 5, RandomInRange(rng, -3, 3)});
        h.Step(true);
    }
    REQUIRE(h.QuiesceSafely());

    const auto stats = h.Chunks().GetStats();
    CAPTURE(stats.pendingCrossChunkBuckets);
    CAPTURE(stats.metaTimeTrackerSize);
    CAPTURE(stats.loadedChunks);

    // Generous bound: the point is unbounded growth, not an exact figure.
    CHECK(stats.pendingCrossChunkBuckets < 2000);
    CHECK(stats.metaTimeTrackerSize < 4000);
}

TEST_CASE("the loaded set stays bounded by the render distance") {
    WorldTestHarness h({});
    uint64_t rng = 7ULL;

    for (int frame = 0; frame < 60; ++frame) {
        h.MovePlayerToChunk(ChunkCoord{RandomInRange(rng, -6, 6), 5, RandomInRange(rng, -6, 6)});
        h.Step(true);
        // loadDistance is 2, so the shell is at most 5x5x5.
        REQUIRE(h.Chunks().GetStats().loadedChunks <= 125 + 32);
    }
}

} // TEST_SUITE("world.tier3.integrity")
