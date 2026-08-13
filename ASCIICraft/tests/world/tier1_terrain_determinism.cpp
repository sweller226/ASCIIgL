// Terrain generation determinism.
//
// These establish that GenerateChunkInto is a pure function of (seed, chunkCoord).
// That matters far beyond terrain: every Tier 3 assertion compares a streamed chunk
// against a freshly generated reference, which is only meaningful if generation is
// reproducible. It also localises the reported bugs - if generation is deterministic
// and single-threaded output matches concurrent output, then corrupted chunks are the
// chunk manager's doing, not the generator's.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>
#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>
#include <ASCIICraft/world/terrain/TerrainResult.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace {

using Blocks = std::vector<uint32_t>;

struct GenOutput {
    Blocks blocks;
    std::vector<WorldBlockPlacement> cross;
};

GenOutput Generate(TerrainGenerator& gen, ChunkCoord coord) {
    GenOutput out;
    out.blocks.assign(Chunk::VOLUME, 0u);
    TerrainResult result;
    gen.GenerateChunkInto(coord, out.blocks.data(), result, &testsupport::SharedBlocks());
    out.cross = std::move(result.crossChunkBlocks);
    return out;
}

bool SamePlacements(const std::vector<WorldBlockPlacement>& a,
                    const std::vector<WorldBlockPlacement>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!(a[i].pos == b[i].pos) || a[i].stateId != b[i].stateId) return false;
    }
    return true;
}

/// Chunk Y layers for this world, measured rather than assumed:
///   0..3  entirely dirt (underground)
///   4     surface transition
///   5     main surface band - terrain, vegetation, tree spill
///   6+    air
/// Sea level is 75, i.e. world Y 75, which lands in chunk Y 4.
constexpr int kSurfaceChunkY = 5;
constexpr int kUndergroundChunkY = 2;
constexpr int kSkyChunkY = 8;

/// Assorted coords across all four bands, including negatives.
const std::vector<ChunkCoord> kCoords = {
    {  0, kSurfaceChunkY,   0}, {  3, kSurfaceChunkY,   5},
    { -2, kSurfaceChunkY,  -3}, {-11, kSurfaceChunkY,  24},
    { 17, 4,             -40}, {  8, kUndergroundChunkY, 8},
    {-64, kSurfaceChunkY,  64}, {  0, kSkyChunkY,        0},
};

} // namespace

TEST_SUITE("world.tier1.terrain") {

TEST_CASE("same seed and coord produce identical blocks and placements") {
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator a(reg, 12345ULL);
    TerrainGenerator b(reg, 12345ULL);

    for (const ChunkCoord c : kCoords) {
        CAPTURE(c.x); CAPTURE(c.y); CAPTURE(c.z);
        const GenOutput ga = Generate(a, c);
        const GenOutput gb = Generate(b, c);
        REQUIRE(ga.blocks == gb.blocks);
        REQUIRE(SamePlacements(ga.cross, gb.cross));
    }
}

TEST_CASE("repeated generation on one instance is stable") {
    // Catches accidental mutable state carried between chunks on the generator.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const GenOutput first = Generate(gen, ChunkCoord{3, kSurfaceChunkY, 5});
    for (int i = 0; i < 4; ++i) {
        // Interleave other coords so any leaked per-chunk state would show up.
        Generate(gen, ChunkCoord{i, kSurfaceChunkY, i});
        const GenOutput again = Generate(gen, ChunkCoord{3, kSurfaceChunkY, 5});
        REQUIRE(again.blocks == first.blocks);
        REQUIRE(SamePlacements(again.cross, first.cross));
    }
}

TEST_CASE("concurrent generation matches the single-threaded reference") {
    // The generator's noise fields are initialised through std::call_once and sampled
    // via const FastNoiseLite methods, so many terrain workers may share one instance.
    // This is the test that actually validates that claim.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const std::vector<ChunkCoord> coords = {
        {1, kSurfaceChunkY, 1}, {2, kSurfaceChunkY, -2},
        {-3, kSurfaceChunkY, 4}, {5, 4, 5},
    };
    std::vector<GenOutput> reference;
    for (const ChunkCoord c : coords) reference.push_back(Generate(gen, c));

    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            for (int iter = 0; iter < 10; ++iter) {
                for (size_t i = 0; i < coords.size(); ++i) {
                    const GenOutput got = Generate(gen, coords[i]);
                    if (got.blocks != reference[i].blocks ||
                        !SamePlacements(got.cross, reference[i].cross)) {
                        mismatches.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    CHECK(mismatches.load() == 0);
}

TEST_CASE("different seeds produce different terrain") {
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator a(reg, 1ULL);
    TerrainGenerator b(reg, 2ULL);

    // Must be a surface-band chunk: underground is dirt regardless of seed and the
    // sky is air regardless of seed, so either would compare equal and prove nothing.
    const ChunkCoord c{3, kSurfaceChunkY, 5};
    const GenOutput ga = Generate(a, c);
    const GenOutput gb = Generate(b, c);

    size_t differing = 0;
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        if (ga.blocks[i] != gb.blocks[i]) ++differing;
    }
    CHECK(differing > 0);
}

TEST_CASE("generation writes nothing outside the block buffer") {
    // Guard band: GenerateChunkInto takes a raw pointer with no length, so an overrun
    // would silently corrupt whatever the caller placed after it.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    constexpr int kGuard = 64;
    constexpr uint32_t kCanary = 0xDEADBEEFu;
    std::vector<uint32_t> buffer(Chunk::VOLUME + kGuard, kCanary);

    TerrainResult result;
    gen.GenerateChunkInto(ChunkCoord{3, 7, 5}, buffer.data(), result, &testsupport::SharedBlocks());

    bool guardIntact = true;
    for (int i = 0; i < kGuard; ++i) {
        if (buffer[Chunk::VOLUME + i] != kCanary) { guardIntact = false; break; }
    }
    CHECK(guardIntact);
}

TEST_CASE("underground chunks are legitimately uniform dirt") {
    // Establishes the baseline for the corruption invariant below. DetermineBlockState
    // returns dirt for every voxel below the surface, so a solid-dirt chunk is CORRECT
    // underground. Any "no uniform chunks" check must be scoped to the surface band or
    // it fires on every underground chunk in the world.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const GenOutput g = Generate(gen, ChunkCoord{3, kUndergroundChunkY, 5});
    const uint32_t dirt = testsupport::Ids().dirt;
    CHECK(std::all_of(g.blocks.begin(), g.blocks.end(),
                      [dirt](uint32_t v) { return v == dirt; }));
}

TEST_CASE("every column is solid below its surface and air above it") {
    // The real corruption invariant, and the one Tier 3 should use.
    //
    // "No uniform chunks" does not work: underground chunks are correctly 100% dirt,
    // sky chunks are correctly 100% air, and because terrain height varies, which
    // chunk Y holds the surface differs per column. What IS universally true is the
    // vertical profile - solid up to the surface, air above it, exactly one
    // transition. A chunk wrongly filled with dirt shows up as solid above the
    // transition; a chunk wrongly left empty shows up as air below it.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);
    const uint32_t air = testsupport::Ids().air;

    constexpr int kMaxChunkY = 8;

    for (int cx = -1; cx <= 1; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            // Stack the whole vertical column of chunks for this (cx, cz).
            std::vector<Blocks> column;
            for (int cy = 0; cy <= kMaxChunkY; ++cy) {
                column.push_back(Generate(gen, ChunkCoord{cx, cy, cz}).blocks);
            }

            for (int lx = 0; lx < sizes::CHUNK_SIZE; ++lx) {
                for (int lz = 0; lz < sizes::CHUNK_SIZE; ++lz) {
                    // Flatten this block column into world-Y order.
                    std::vector<uint32_t> profile;
                    profile.reserve((kMaxChunkY + 1) * sizes::CHUNK_SIZE);
                    for (int cy = 0; cy <= kMaxChunkY; ++cy) {
                        for (int ly = 0; ly < sizes::CHUNK_SIZE; ++ly) {
                            profile.push_back(column[cy][chunkutil::GetBlockIndex(lx, ly, lz)]);
                        }
                    }

                    int highestSolid = -1;
                    for (int i = static_cast<int>(profile.size()) - 1; i >= 0; --i) {
                        if (profile[i] != air) { highestSolid = i; break; }
                    }
                    REQUIRE(highestSolid >= 0);   // no column may be entirely air

                    // Everything at or below the surface is solid, everything above is air.
                    for (int i = 0; i <= highestSolid; ++i) {
                        if (profile[i] == air) {
                            CAPTURE(cx); CAPTURE(cz); CAPTURE(lx); CAPTURE(lz);
                            CAPTURE(i); CAPTURE(highestSolid);
                            FAIL_CHECK("air pocket below the surface");
                            break;
                        }
                    }
                    for (size_t i = highestSolid + 1; i < profile.size(); ++i) {
                        if (profile[i] != air) {
                            CAPTURE(cx); CAPTURE(cz); CAPTURE(lx); CAPTURE(lz);
                            FAIL_CHECK("solid block floating above the surface");
                            break;
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("the surface band emits cross-chunk placements") {
    // Vegetation and trees are delivered through TerrainResult::crossChunkBlocks, so a
    // surface chunk producing none would mean decoration silently stopped.
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, 12345ULL);

    const GenOutput g = Generate(gen, ChunkCoord{3, kSurfaceChunkY, 5});
    CHECK_FALSE(g.cross.empty());
}

} // TEST_SUITE("world.tier1.terrain")
