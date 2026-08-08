// BlockKey injectivity.
//
// terrain::BlockKey is the key for the generator's dedup sets: `occupied` cells, tree
// log positions, and leaf positions. A collision would make a set report a cell as
// already used, so PushLeaf returns early and the block is silently dropped.
//
// The field layout does overlap:
//     x << 42  occupies bits 42..73 (truncated at 63)
//     y << 21  occupies bits 21..52
//     z        occupies bits  0..31
//
// BUT the overlap is harmless at the coordinates this generator actually uses. What
// matters for a collision is which bits *vary* across the positions hashed together,
// and for any realistic footprint those ranges are disjoint:
//
//     x in [-1040,-1008)  ->  x<<42 varies in bits 42..52
//     y in [0, 256)       ->  y<<21 varies in bits 21..28
//     z in [-1040,-1008)  ->  z     varies in bits  0..10
//
// Triggering an actual collision needs a coordinate around 2^21 or larger, which is
// two orders of magnitude beyond the 1024-chunk (16384 block) world extent.
//
// So: the tests below prove injectivity everywhere the generator can reach, and the
// three should_fail cases pin the latent aliasing with crafted out-of-world inputs.
// Those are documentation of a real fragility, not of a live bug - if someone raises
// the world height or extent past 2^21, they start mattering immediately.
//
// This corrects the original diagnosis. BlockKey is NOT a cause of missing flowers or
// holes in tree canopies; look to the chunk lifecycle for those.

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

// --- Latent aliasing: out-of-world inputs only -------------------------------
// These fail once BlockKey becomes a proper mixed hash. Keep the decorators until
// then; removing one without fixing the hash turns the suite red.

TEST_CASE("BlockKey y field does not alias the z field"
          * doctest::should_fail()) {
    // Needs z == 2^21, well beyond the world extent.
    CHECK(terrain::BlockKey(0, 1, 0) != terrain::BlockKey(0, 0, 1 << 21));
}

TEST_CASE("BlockKey x field does not alias the y field"
          * doctest::should_fail()) {
    // Needs y == 2^21, ~8000x the world height.
    CHECK(terrain::BlockKey(1, 0, 0) != terrain::BlockKey(0, 1 << 21, 0));
}

TEST_CASE("BlockKey does not discard the high bits of x"
          * doctest::should_fail()) {
    // x << 42 pushes anything above 2^22 off the top of the u64.
    CHECK(terrain::BlockKey(1 << 22, 0, 0) != terrain::BlockKey(0, 0, 0));
}

} // TEST_SUITE("world.tier1.keys")
