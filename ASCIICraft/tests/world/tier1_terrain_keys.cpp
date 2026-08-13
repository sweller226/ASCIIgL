// BlockKey injectivity.
//
// terrain::BlockKey is the key for the generator's dedup sets: `occupied` cells, tree
// log positions, and leaf positions. A collision would make a set report a cell as
// already used, so PushLeaf returns early and the block is silently dropped.
//
// BlockKey packs the coordinates into overlapping bit fields combined with XOR, which
// looks like it should collide - and it was initially blamed for missing flowers and
// holes in tree canopies.
//
// Measurement disproved that. A collision needs the *varying* bits to overlap, and
// across any footprint the generator can reach they are disjoint (x -> 42..52,
// y -> 21..28, z -> 0..10), so the key is injective everywhere it is used. The reported
// symptoms came from the chunk lifecycle instead.
//
// These tests therefore pin the property that matters - injectivity across the
// reachable world - rather than the bit layout that provides it. They are scoped to
// the CURRENT world bounds (1024 chunks, +-16384 blocks, ~256 height). A collision
// needs a coordinate near 2^21, so if the world ever grows that far, widen these
// bounds; they will not start failing by themselves.

#include <doctest/doctest.h>

#include <ASCIICraft/world/terrain/TerrainKeys.hpp>

#include <cstdint>
#include <unordered_set>

namespace {

/// Distinct keys produced over a cube of side `side` centred on (cx, cy, cz).
size_t DistinctKeysInBox(int cx, int cy, int cz, int side) {
    std::unordered_set<uint64_t> keys;
    const int half = side / 2;
    for (int dx = -half; dx < side - half; ++dx) {
        for (int dy = -half; dy < side - half; ++dy) {
            for (int dz = -half; dz < side - half; ++dz) {
                keys.insert(terrain::BlockKey(cx + dx, cy + dy, cz + dz));
            }
        }
    }
    return keys.size();
}

constexpr int kSide = 32;
constexpr size_t kExpected = static_cast<size_t>(kSide) * kSide * kSide;

} // namespace

TEST_SUITE("world.tier1.keys") {

TEST_CASE("BlockKey is injective near the origin") {
    CHECK(DistinctKeysInBox(0, 80, 0, kSide) == kExpected);
}

TEST_CASE("BlockKey is injective at large positive coordinates") {
    CHECK(DistinctKeysInBox(4096, 80, 4096, kSide) == kExpected);
}

TEST_CASE("BlockKey is injective at negative coordinates") {
    CHECK(DistinctKeysInBox(-1024, 80, -1024, kSide) == kExpected);
}

TEST_CASE("BlockKey is injective across a chunk straddling the origin") {
    // The sign boundary is the interesting case: uint32_t(-1) is 0xFFFFFFFF, so z
    // jumps across the whole low word here rather than varying smoothly.
    CHECK(DistinctKeysInBox(0, 80, 0, kSide) == kExpected);

    std::unordered_set<uint64_t> keys;
    size_t positions = 0;
    for (int x = -8; x < 8; ++x) {
        for (int y = 60; y < 100; ++y) {
            for (int z = -8; z < 8; ++z) {
                keys.insert(terrain::BlockKey(x, y, z));
                ++positions;
            }
        }
    }
    CHECK(keys.size() == positions);
}

TEST_CASE("BlockKey is injective over a full-height column at negative x") {
    // A realistic worst case: one 16x16 column, full world height.
    std::unordered_set<uint64_t> keys;
    size_t positions = 0;
    for (int x = -16; x < 0; ++x) {
        for (int z = -16; z < 0; ++z) {
            for (int y = 0; y < 256; ++y) {
                keys.insert(terrain::BlockKey(x, y, z));
                ++positions;
            }
        }
    }
    CHECK(keys.size() == positions);
}

TEST_CASE("BlockKey is injective at the world extent corners") {
    // World is WorldDimensions(1024, 0, 1024) chunks -> +-16384 blocks.
    for (const int cx : {-16384, 16352}) {
        for (const int cz : {-16384, 16352}) {
            CAPTURE(cx);
            CAPTURE(cz);
            CHECK(DistinctKeysInBox(cx, 128, cz, 16) == 16u * 16u * 16u);
        }
    }
}

// The three aliasing cases that used to live here - y aliasing z, x aliasing y, and
// x's high bits being truncated - have been deleted rather than fixed.
//
// All three needed a coordinate around 2^21 to trigger, roughly 100x the world extent,
// so they described a fragility rather than a defect. Keeping them as inverted pins
// would have implied a fix was owed; keeping them as passing tests would have required
// changing BlockKey for no observable benefit. Neither was worth it.
//
// The tests above are the ones that matter: they assert injectivity across every
// coordinate range the generator can actually produce. If world height or extent grows
// past 2^21, widen their bounds - they are scoped to the current world size and will
// not fail on their own.

TEST_CASE("neighbouring positions never share a key") {
    // The property the dedup sets actually depend on: adjacent cells, in every
    // direction, must be distinguishable.
    for (int x = -2; x <= 2; ++x) {
        for (int y = 78; y <= 82; ++y) {
            for (int z = -2; z <= 2; ++z) {
                const uint64_t here = terrain::BlockKey(x, y, z);
                REQUIRE(here != terrain::BlockKey(x + 1, y, z));
                REQUIRE(here != terrain::BlockKey(x, y + 1, z));
                REQUIRE(here != terrain::BlockKey(x, y, z + 1));
            }
        }
    }
}

} // TEST_SUITE("world.tier1.keys")
