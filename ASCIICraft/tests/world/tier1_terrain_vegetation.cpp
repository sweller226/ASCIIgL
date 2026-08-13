// Vegetation placement.
//
// The statistical net for "multiple chunks of flowers don't generate". These assert
// at the GENERATOR level: if they pass, decoration is being produced correctly and a
// flowerless world is the chunk manager losing it, not the generator failing to make
// it.
//
// Deliberately free of recorded magic numbers. Absolute counts would need
// recalibrating on every noise tweak and would rot; these compare the world against
// itself instead - every region must have vegetation, and negative coordinates must
// behave like positive ones.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>
#include <ASCIICraft/world/terrain/TerrainResult.hpp>

#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace {

/// Chunk Y layers that can hold surface decoration for this world. Terrain height
/// varies per column and trees extend well above it (a giant oak is up to 22 blocks
/// of trunk plus canopy), so decoration spans several layers.
constexpr int kSurfaceLayers[] = {3, 4, 5, 6, 7};

struct VegCounts {
    size_t flowers = 0;     // poppy + dandelion
    size_t groundCover = 0; // tall_grass + fern
    size_t logs = 0;
    size_t leaves = 0;
    size_t total = 0;
};

/// Biome noise runs at frequency 0.0025, so a biome spans roughly 400 blocks - about
/// 25 chunks. Any test sampling a contiguous block smaller than that can land entirely
/// inside one biome and see none of another biome's decoration. Sampling with a stride
/// covers many biomes for the same number of generated chunks.
constexpr int kBiomeStride = 24;

/// Collects cross-chunk placements over a grid of chunk columns.
/// \param stride gap between sampled columns; 1 for a contiguous block.
VegCounts SampleArea(TerrainGenerator& gen, int cx0, int cz0, int extent,
                     std::set<std::tuple<int, int, int>>* positionsOut = nullptr,
                     int stride = 1) {
    const auto& ids = testsupport::Ids();
    VegCounts counts;
    std::vector<uint32_t> blocks(Chunk::VOLUME);

    for (int cx = cx0; cx < cx0 + extent * stride; cx += stride) {
        for (int cz = cz0; cz < cz0 + extent * stride; cz += stride) {
            for (const int cy : kSurfaceLayers) {
                TerrainResult result;
                gen.GenerateChunkInto(ChunkCoord{cx, cy, cz}, blocks.data(), result,
                                      &testsupport::SharedBlocks());
                for (const auto& p : result.crossChunkBlocks) {
                    ++counts.total;
                    if (p.stateId == ids.poppy || p.stateId == ids.dandelion) ++counts.flowers;
                    else if (p.stateId == ids.tallGrass || p.stateId == ids.fern) ++counts.groundCover;
                    else if (p.stateId == ids.oakLeaves) ++counts.leaves;
                    else if (p.stateId == ids.oakLog) ++counts.logs;

                    if (positionsOut) {
                        positionsOut->insert({p.pos.x, p.pos.y, p.pos.z});
                    }
                }
            }
        }
    }
    return counts;
}

} // namespace

TEST_SUITE("world.tier1.vegetation") {

TEST_CASE("every vegetation type appears across a multi-biome sample") {
    // Strided so the sample spans many biomes. A contiguous 8x8 block sits inside a
    // single biome and legitimately contains zero flowers - which is precisely how a
    // naive version of this test produced a false "flowers never generate" result.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const VegCounts c = SampleArea(gen, -96, -96, 8, nullptr, kBiomeStride);
    CAPTURE(c.total);
    CAPTURE(c.flowers);
    CAPTURE(c.groundCover);
    CAPTURE(c.logs);
    CAPTURE(c.leaves);

    CHECK(c.total > 0);
    CHECK(c.flowers > 0);
    CHECK(c.groundCover > 0);
    CHECK(c.logs > 0);
    CHECK(c.leaves > 0);
}

TEST_CASE("flowers appear in a meaningful fraction of a wide sample") {
    // Guards the reported symptom at the generator: if flower emission broke entirely,
    // this drops to zero. Band is wide because the flower/forest biome split varies.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const VegCounts c = SampleArea(gen, -120, -120, 10, nullptr, kBiomeStride);
    REQUIRE(c.total > 0);

    const double flowerFraction = static_cast<double>(c.flowers) / static_cast<double>(c.total);
    CAPTURE(c.flowers);
    CAPTURE(c.total);
    CAPTURE(flowerFraction);
    CHECK(c.flowers > 0);
    CHECK(flowerFraction > 0.001);   // observed ~1-2% across the world
}

TEST_CASE("no 4x4 chunk window is devoid of vegetation") {
    // The direct analogue of the reported symptom: flowerless chunks appearing in
    // contiguous groups. A whole 4x4 block of chunks with no decoration at all would
    // be visible as a dead zone.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    for (int wx = 0; wx < 8; wx += 4) {
        for (int wz = 0; wz < 8; wz += 4) {
            const VegCounts c = SampleArea(gen, wx, wz, 4);
            CAPTURE(wx);
            CAPTURE(wz);
            REQUIRE(c.total > 0);
            REQUIRE(c.flowers + c.groundCover > 0);
        }
    }
}

TEST_CASE("negative coordinates produce comparable vegetation density") {
    // Was previously suspected as a BlockKey collision symptom. tier1_terrain_keys.cpp
    // showed the hash is injective across the reachable world, so this should hold -
    // and it now guards against any future hash change that would break it.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const VegCounts pos = SampleArea(gen, 0, 0, 8);
    const VegCounts neg = SampleArea(gen, -8, -8, 8);
    CAPTURE(pos.total);
    CAPTURE(neg.total);

    REQUIRE(pos.total > 0);
    REQUIRE(neg.total > 0);

    // Biomes differ between regions, so allow a wide band; this is an order-of-
    // magnitude check, not a distribution test.
    const double ratio = static_cast<double>(neg.total) / static_cast<double>(pos.total);
    CAPTURE(ratio);
    CHECK(ratio > 0.25);
    CHECK(ratio < 4.0);
}

TEST_CASE("placements may target the same position, and order decides the winner") {
    // Characterization. Duplicates are common and intentional: ground cover, flowers
    // and tree blocks are concatenated in a fixed order, with trees appended LAST so
    // a trunk overwrites a flower rather than growing through it.
    //
    // The invariant that matters is therefore not uniqueness but ORDER STABILITY -
    // the same generation must always produce the same final block at a contested
    // cell. If the concatenation order ever changed, decoration would visibly shuffle.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator a(reg, 12345ULL);
    TerrainGenerator b(reg, 12345ULL);
    std::vector<uint32_t> blocks(Chunk::VOLUME);

    size_t contested = 0;
    for (int cx = 0; cx < 4; ++cx) {
        for (int cz = 0; cz < 4; ++cz) {
            for (const int cy : kSurfaceLayers) {
                TerrainResult ra, rb;
                a.GenerateChunkInto(ChunkCoord{cx, cy, cz}, blocks.data(), ra,
                                    &testsupport::SharedBlocks());
                b.GenerateChunkInto(ChunkCoord{cx, cy, cz}, blocks.data(), rb,
                                    &testsupport::SharedBlocks());

                REQUIRE(ra.crossChunkBlocks.size() == rb.crossChunkBlocks.size());

                // Last-write-wins resolution must agree between the two runs.
                std::map<std::tuple<int, int, int>, uint32_t> finalA, finalB;
                std::set<std::tuple<int, int, int>> seen;
                for (const auto& p : ra.crossChunkBlocks) {
                    const auto key = std::make_tuple(p.pos.x, p.pos.y, p.pos.z);
                    if (!seen.insert(key).second) ++contested;
                    finalA[key] = p.stateId;
                }
                for (const auto& p : rb.crossChunkBlocks) {
                    finalB[std::make_tuple(p.pos.x, p.pos.y, p.pos.z)] = p.stateId;
                }
                REQUIRE(finalA == finalB);
            }
        }
    }
    CAPTURE(contested);
    CHECK(contested > 0);   // if this ever hits zero, the overlap rules changed
}

TEST_CASE("vegetation placement is deterministic across runs") {
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator a(reg, 12345ULL);
    TerrainGenerator b(reg, 12345ULL);

    std::set<std::tuple<int, int, int>> posA, posB;
    const VegCounts ca = SampleArea(a, 0, 0, 4, &posA);
    const VegCounts cb = SampleArea(b, 0, 0, 4, &posB);

    CHECK(ca.total == cb.total);
    CHECK(ca.flowers == cb.flowers);
    CHECK(posA == posB);
}

} // TEST_SUITE("world.tier1.vegetation")
