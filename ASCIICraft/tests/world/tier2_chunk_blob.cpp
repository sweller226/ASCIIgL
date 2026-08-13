// Chunk blob serialization round-trips.
//
// The v2 blob is a palette of (name, properties) strings plus bit-packed indices at
// 4, 8 or 16 bits depending on palette size. The width transitions at 16 and 256 are
// the interesting boundaries - an off-by-one there silently transposes a chunk.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>

#include <random>
#include <vector>

namespace {

/// Fills a chunk cycling through `distinctStates` different state ids, so the palette
/// ends up exactly that size.
void FillWithDistinctStates(Chunk& chunk, uint32_t distinctStates) {
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        chunk.SetBlockStateByIndex(i, static_cast<uint32_t>(i) % distinctStates);
    }
    chunk.SetGenerated(true);
}

/// The save path refuses ungenerated chunks, so any chunk a test persists has to look
/// like one whose terrain actually ran.
void MarkGenerated(Chunk& c) { c.SetGenerated(true); }

bool SameBlocks(const Chunk& a, const Chunk& b) {
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        if (a.GetBlockStateByIndex(i) != b.GetBlockStateByIndex(i)) return false;
    }
    return true;
}

/// Save `src`, then load it back into `dst`.
/// Chunk owns unique_ptr mesh members, so it is neither copyable nor movable - the
/// result has to come back through an out-parameter rather than by value.
void RoundTrip(const Chunk& src, const std::filesystem::path& dir, Chunk& dst) {
    const ChunkCoord coord = src.GetCoord();
    {
        RegionFile region(coord.ToRegionCoord(), dir);
        REQUIRE(region.SaveChunk(&src, testsupport::SharedBlocks()));
    }
    RegionFile region(coord.ToRegionCoord(), dir);
    REQUIRE(region.LoadChunk(&dst, testsupport::SharedBlocks()));
}

} // namespace

TEST_SUITE("world.tier2.chunkblob") {

TEST_CASE("round-trips across the 4-bit palette range") {
    testsupport::TempDir dir("blob4");
    // indexBits == 4 while the palette holds 16 or fewer distinct states.
    for (const uint32_t n : {1u, 2u, 15u, 16u}) {
        CAPTURE(n);
        Chunk src(ChunkCoord{0, 0, 0});
        MarkGenerated(src);
        FillWithDistinctStates(src, n);
        Chunk dst(src.GetCoord());
        RoundTrip(src, dir.Path(), dst);
        REQUIRE(SameBlocks(src, dst));
    }
}

TEST_CASE("round-trips across the 8-bit palette range") {
    testsupport::TempDir dir("blob8");
    // 17 crosses out of 4-bit; 220 is every state the vanilla registry defines.
    for (const uint32_t n : {17u, 100u, 220u}) {
        CAPTURE(n);
        Chunk src(ChunkCoord{1, 2, 3});
        MarkGenerated(src);
        FillWithDistinctStates(src, n);
        Chunk dst(src.GetCoord());
        RoundTrip(src, dir.Path(), dst);
        REQUIRE(SameBlocks(src, dst));
    }
}

TEST_CASE("the 16-bit palette path is unreachable with the current block set") {
    // Documents a coverage gap rather than asserting behaviour. indexBits only reaches
    // 16 above 256 distinct states in one chunk, but the registry defines 220 total,
    // so no chunk can produce that palette. If the block set grows past 256 states the
    // 16-bit packing path goes live for the first time, untested - this test starts
    // failing then, which is the signal to add real coverage for it.
    CHECK(testsupport::SharedBlocks().GetTotalStateCount() <= 256);
}

TEST_CASE("an all-air chunk round-trips") {
    testsupport::TempDir dir("blobair");
    Chunk src(ChunkCoord{4, 5, 6});   // constructor fills with air
    MarkGenerated(src);
    Chunk dst(src.GetCoord());
        RoundTrip(src, dir.Path(), dst);
    CHECK(SameBlocks(src, dst));
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        REQUIRE(dst.GetBlockStateByIndex(i) == testsupport::Ids().air);
    }
}

TEST_CASE("blocks with properties keep their variant across a round-trip") {
    // The palette stores name + serialized properties, so a non-default variant must
    // survive. Dropping properties would silently reset log axes, slab halves, etc.
    testsupport::TempDir dir("blobprops");
    const auto& bsr = testsupport::SharedBlocks();

    const uint32_t logY = bsr.GetDefaultState("minecraft:oak_log");
    const uint32_t logX = bsr.WithProperty(logY, "axis", "x");
    const uint32_t logZ = bsr.WithProperty(logY, "axis", "z");
    REQUIRE(logX != logY);
    REQUIRE(logZ != logY);

    Chunk src(ChunkCoord{7, 1, 7});

    MarkGenerated(src);
    src.SetBlockState(1, 1, 1, logY);
    src.SetBlockState(2, 1, 1, logX);
    src.SetBlockState(3, 1, 1, logZ);

    Chunk dst(src.GetCoord());
        RoundTrip(src, dir.Path(), dst);
    CHECK(dst.GetBlockState(1, 1, 1) == logY);
    CHECK(dst.GetBlockState(2, 1, 1) == logX);
    CHECK(dst.GetBlockState(3, 1, 1) == logZ);
}

TEST_CASE("a randomized chunk round-trips") {
    testsupport::TempDir dir("blobrand");
    std::mt19937 rng(20260808u);
    const uint32_t stateCount = testsupport::SharedBlocks().GetTotalStateCount();
    std::uniform_int_distribution<uint32_t> pick(0, stateCount - 1);

    for (int iteration = 0; iteration < 20; ++iteration) {
        CAPTURE(iteration);
        Chunk src(ChunkCoord{iteration % 8, 0, iteration / 8});
        MarkGenerated(src);
        for (int i = 0; i < Chunk::VOLUME; ++i) {
            src.SetBlockStateByIndex(i, pick(rng));
        }
        Chunk dst(src.GetCoord());
        RoundTrip(src, dir.Path(), dst);
        REQUIRE(SameBlocks(src, dst));
    }
}

TEST_CASE("saving the same chunk twice yields the newest content") {
    testsupport::TempDir dir("blobresave");
    const ChunkCoord coord{2, 3, 4};

    Chunk v1(coord);

    MarkGenerated(v1);
    v1.SetBlockState(0, 0, 0, testsupport::Ids().stone);
    {
        RegionFile region(coord.ToRegionCoord(), dir.Path());
        REQUIRE(region.SaveChunk(&v1, testsupport::SharedBlocks()));
    }

    Chunk v2(coord);

    MarkGenerated(v2);
    v2.SetBlockState(0, 0, 0, testsupport::Ids().glass);
    {
        RegionFile region(coord.ToRegionCoord(), dir.Path());
        REQUIRE(region.SaveChunk(&v2, testsupport::SharedBlocks()));
    }

    Chunk loaded(coord);
    RegionFile region(coord.ToRegionCoord(), dir.Path());
    REQUIRE(region.LoadChunk(&loaded, testsupport::SharedBlocks()));
    CHECK(loaded.GetBlockState(0, 0, 0) == testsupport::Ids().glass);
}

TEST_CASE("chunks at both region-index corners round-trip independently") {
    // Local (0,0,0) and (31,31,31) sit at the ends of the 32768-entry index table.
    testsupport::TempDir dir("blobcorners");
    const RegionCoord region{0, 0, 0};

    Chunk low(ChunkCoord{0, 0, 0});

    MarkGenerated(low);
    low.SetBlockState(0, 0, 0, testsupport::Ids().stone);
    Chunk high(ChunkCoord{31, 31, 31});
    MarkGenerated(high);
    high.SetBlockState(0, 0, 0, testsupport::Ids().glass);

    {
        RegionFile rf(region, dir.Path());
        REQUIRE(rf.SaveChunk(&low, testsupport::SharedBlocks()));
        REQUIRE(rf.SaveChunk(&high, testsupport::SharedBlocks()));
    }

    RegionFile rf(region, dir.Path());
    Chunk loadedLow(ChunkCoord{0, 0, 0});
    Chunk loadedHigh(ChunkCoord{31, 31, 31});
    REQUIRE(rf.LoadChunk(&loadedLow, testsupport::SharedBlocks()));
    REQUIRE(rf.LoadChunk(&loadedHigh, testsupport::SharedBlocks()));

    CHECK(loadedLow.GetBlockState(0, 0, 0) == testsupport::Ids().stone);
    CHECK(loadedHigh.GetBlockState(0, 0, 0) == testsupport::Ids().glass);
}

TEST_CASE("negative chunk coordinates map to the right region and slot") {
    testsupport::TempDir dir("blobneg");
    const ChunkCoord coord{-1, 0, -1};
    REQUIRE(coord.ToRegionCoord() == RegionCoord{-1, 0, -1});

    Chunk src(coord);

    MarkGenerated(src);
    src.SetBlockState(5, 5, 5, testsupport::Ids().stone);
    Chunk dst(src.GetCoord());
        RoundTrip(src, dir.Path(), dst);

    CHECK(dst.GetBlockState(5, 5, 5) == testsupport::Ids().stone);
    CHECK(std::filesystem::exists(dir / "r_-1.0.-1"));
}

TEST_CASE("loading an absent chunk reports absence without throwing") {
    testsupport::TempDir dir("blobmissing");
    RegionFile region(RegionCoord{0, 0, 0}, dir.Path());
    Chunk dst(ChunkCoord{9, 9, 9});
    CHECK_FALSE(region.LoadChunk(&dst, testsupport::SharedBlocks()));
}

} // TEST_SUITE("world.tier2.chunkblob")
