// Coordinate arithmetic.
//
// Cheap, exhaustive, and worth having: nearly every chunk bug class starts with an
// off-by-one or a division that truncates toward zero instead of flooring. Negative
// coordinates are covered deliberately - that is where truncating division breaks.

#include <doctest/doctest.h>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

#include <cstdint>
#include <unordered_set>

namespace {

/// Reference floor division, computed in int64 to avoid the very bug under test.
int64_t FloorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

} // namespace

TEST_SUITE("world.tier1.coords") {

TEST_CASE("WorldCoord to ChunkCoord floors toward negative infinity") {
    for (int v = -64; v <= 64; ++v) {
        const ChunkCoord c = WorldCoord(v, v, v).ToChunkCoord();
        const int expected = static_cast<int>(FloorDiv(v, sizes::CHUNK_SIZE));
        CHECK(c.x == expected);
        CHECK(c.y == expected);
        CHECK(c.z == expected);
    }
}

TEST_CASE("world -> chunk + local round-trips exactly") {
    // The invariant the whole streaming system rests on: a world position decomposes
    // into a chunk and an in-range local offset that recombine to the original.
    for (int x = -40; x <= 40; ++x) {
        for (int y = -40; y <= 40; ++y) {
            for (int z = -40; z <= 40; ++z) {
                const WorldCoord w(x, y, z);
                const ChunkCoord c = w.ToChunkCoord();
                const glm::ivec3 l = w.ToLocalChunkPos();

                REQUIRE(l.x >= 0); REQUIRE(l.x < sizes::CHUNK_SIZE);
                REQUIRE(l.y >= 0); REQUIRE(l.y < sizes::CHUNK_SIZE);
                REQUIRE(l.z >= 0); REQUIRE(l.z < sizes::CHUNK_SIZE);

                REQUIRE(c.x * sizes::CHUNK_SIZE + l.x == x);
                REQUIRE(c.y * sizes::CHUNK_SIZE + l.y == y);
                REQUIRE(c.z * sizes::CHUNK_SIZE + l.z == z);
            }
        }
    }
}

TEST_CASE("ChunkCoord to RegionCoord floors, and local region offsets round-trip") {
    for (int v = -96; v <= 96; ++v) {
        const ChunkCoord c(v, v, v);
        const RegionCoord r = c.ToRegionCoord();
        const int expected = static_cast<int>(FloorDiv(v, sizes::REGION_SIZE));
        REQUIRE(r.x == expected);

        const glm::ivec3 l = c.ToLocalRegion(r);
        REQUIRE(l.x >= 0); REQUIRE(l.x < sizes::REGION_SIZE);
        REQUIRE(r.x * sizes::REGION_SIZE + l.x == v);
        REQUIRE(r.y * sizes::REGION_SIZE + l.y == v);
        REQUIRE(r.z * sizes::REGION_SIZE + l.z == v);
    }
}

TEST_CASE("region index offsets are unique across a whole region") {
    // RegionFile addresses chunks by x + y*32 + z*32*32. Any collision would alias two
    // chunks onto one slot and silently overwrite saved data.
    std::unordered_set<int> seen;
    for (int z = 0; z < sizes::REGION_SIZE; ++z) {
        for (int y = 0; y < sizes::REGION_SIZE; ++y) {
            for (int x = 0; x < sizes::REGION_SIZE; ++x) {
                const int idx = x + y * sizes::REGION_SIZE + z * sizes::REGION_SIZE * sizes::REGION_SIZE;
                REQUIRE(idx >= 0);
                REQUIRE(idx < sizes::REGION_SIZE * sizes::REGION_SIZE * sizes::REGION_SIZE);
                REQUIRE(seen.insert(idx).second);
            }
        }
    }
    CHECK(seen.size() == static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE);
}

TEST_CASE("ChebyshevDistance is symmetric and matches max component delta") {
    const ChunkCoord origin{0, 0, 0};
    CHECK(ChebyshevDistance(origin, origin) == 0);

    for (int dx = -5; dx <= 5; ++dx) {
        for (int dy = -5; dy <= 5; ++dy) {
            for (int dz = -5; dz <= 5; ++dz) {
                const ChunkCoord a{dx, dy, dz};
                const int expected = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
                REQUIRE(ChebyshevDistance(a, origin) == expected);
                REQUIRE(ChebyshevDistance(origin, a) == ChebyshevDistance(a, origin));
            }
        }
    }
}

TEST_CASE("stepping to a neighbour and back returns to the original chunk") {
    // Pins the reverse-direction formula UnloadChunk relies on: (i%2==0) ? i+1 : i-1.
    // FaceDir is ordered in opposing pairs (Top/Bottom, North/South, East/West), so
    // that arithmetic is only correct while the enum keeps that layout.
    const ChunkCoord start{3, 7, -2};
    for (int i = 0; i < kFaceCount; ++i) {
        const int reverse = (i % 2 == 0) ? i + 1 : i - 1;
        const ChunkCoord there = NeighborChunkCoord(start, FaceDirFromIndex(i));
        const ChunkCoord back  = NeighborChunkCoord(there, FaceDirFromIndex(reverse));
        REQUIRE(there != start);
        REQUIRE(back == start);
    }
}

TEST_CASE("ChunkCoord hashing has no collisions over a large box") {
    // loadedChunks is an unordered_map keyed by ChunkCoord; heavy collisions would be
    // a silent performance cliff during streaming rather than a visible failure.
    std::unordered_set<size_t> hashes;
    std::hash<ChunkCoord> hasher;
    size_t count = 0;
    for (int x = -20; x < 20; ++x) {
        for (int y = -20; y < 20; ++y) {
            for (int z = -20; z < 20; ++z) {
                hashes.insert(hasher(ChunkCoord{x, y, z}));
                ++count;
            }
        }
    }
    CHECK(count == 64000);
    CHECK(hashes.size() == count);
}

} // TEST_SUITE("world.tier1.coords")
