// Block indexing and cross-chunk edit packing.
//
// The x + y*16 + z*256 index formula is written out by hand in several places
// (ChunkUtil, the terrain generator, the region blob parser). If any copy disagrees,
// chunks silently transpose on save/load. These tests pin the canonical ordering.

#include <doctest/doctest.h>

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>
#include <ASCIICraft/world/chunk/CrossChunkEdit.hpp>

#include <unordered_set>
#include <vector>

TEST_SUITE("world.tier1.chunkutil") {

TEST_CASE("GetBlockIndex is a bijection over the chunk volume") {
    std::vector<bool> seen(Chunk::VOLUME, false);
    for (int z = 0; z < sizes::CHUNK_SIZE; ++z) {
        for (int y = 0; y < sizes::CHUNK_SIZE; ++y) {
            for (int x = 0; x < sizes::CHUNK_SIZE; ++x) {
                const int i = chunkutil::GetBlockIndex(x, y, z);
                REQUIRE(i >= 0);
                REQUIRE(i < Chunk::VOLUME);
                REQUIRE_FALSE(seen[i]);
                seen[i] = true;
            }
        }
    }
    for (int i = 0; i < Chunk::VOLUME; ++i) REQUIRE(seen[i]);
}

TEST_CASE("index inverse recovers the original coordinate") {
    // The ordering is X-fastest, then Y, then Z - not the usual Minecraft YZX. Any
    // code inverting an index must use this decomposition.
    for (int i = 0; i < Chunk::VOLUME; ++i) {
        const int x = i % sizes::CHUNK_SIZE;
        const int y = (i / sizes::CHUNK_SIZE) % sizes::CHUNK_SIZE;
        const int z = i / (sizes::CHUNK_SIZE * sizes::CHUNK_SIZE);
        REQUIRE(chunkutil::GetBlockIndex(x, y, z) == i);
    }
}

TEST_CASE("IsValidBlockCoord accepts exactly the in-range cube") {
    CHECK(chunkutil::IsValidBlockCoord(0, 0, 0));
    CHECK(chunkutil::IsValidBlockCoord(15, 15, 15));
    CHECK_FALSE(chunkutil::IsValidBlockCoord(-1, 0, 0));
    CHECK_FALSE(chunkutil::IsValidBlockCoord(0, -1, 0));
    CHECK_FALSE(chunkutil::IsValidBlockCoord(0, 0, -1));
    CHECK_FALSE(chunkutil::IsValidBlockCoord(16, 0, 0));
    CHECK_FALSE(chunkutil::IsValidBlockCoord(0, 16, 0));
    CHECK_FALSE(chunkutil::IsValidBlockCoord(0, 0, 16));
}

TEST_CASE("CrossChunkEdit packs and unpacks every in-chunk position") {
    std::unordered_set<uint16_t> packedValues;
    for (int z = 0; z < sizes::CHUNK_SIZE; ++z) {
        for (int y = 0; y < sizes::CHUNK_SIZE; ++y) {
            for (int x = 0; x < sizes::CHUNK_SIZE; ++x) {
                CrossChunkEdit e{};
                e.PackPos(x, y, z);
                REQUIRE(e.packedPos < 4096);   // 12 bits used, 4 reserved
                REQUIRE(packedValues.insert(e.packedPos).second);

                int ux = -1, uy = -1, uz = -1;
                e.UnpackPos(ux, uy, uz);
                REQUIRE(ux == x);
                REQUIRE(uy == y);
                REQUIRE(uz == z);
            }
        }
    }
    CHECK(packedValues.size() == static_cast<size_t>(Chunk::VOLUME));
}

TEST_CASE("CrossChunkEdit masks out-of-range coordinates rather than rejecting them") {
    // Characterization, not endorsement. PackPos keeps only 4 bits per axis, so an
    // out-of-range coordinate silently wraps. Every caller currently derives its input
    // from ToLocalChunkPos (always 0..15), so this is unreachable today - but it is a
    // trap for any future caller, and the behaviour should be noticed if it changes.
    CrossChunkEdit e{};
    e.PackPos(16, 17, 18);
    int x = 0, y = 0, z = 0;
    e.UnpackPos(x, y, z);
    CHECK(x == 0);
    CHECK(y == 1);
    CHECK(z == 2);
}

TEST_CASE("IsOnChunkFaceBoundary agrees with a reference predicate") {
    for (int z = 0; z < sizes::CHUNK_SIZE; ++z) {
        for (int y = 0; y < sizes::CHUNK_SIZE; ++y) {
            for (int x = 0; x < sizes::CHUNK_SIZE; ++x) {
                const glm::ivec3 p{x, y, z};
                const int last = sizes::CHUNK_SIZE - 1;
                REQUIRE(chunkutil::IsOnChunkFaceBoundary(p, FaceDir::Top)    == (y == last));
                REQUIRE(chunkutil::IsOnChunkFaceBoundary(p, FaceDir::Bottom) == (y == 0));
                REQUIRE(chunkutil::IsOnChunkFaceBoundary(p, FaceDir::North)  == (z == 0));
                REQUIRE(chunkutil::IsOnChunkFaceBoundary(p, FaceDir::South)  == (z == last));
                REQUIRE(chunkutil::IsOnChunkFaceBoundary(p, FaceDir::East)   == (x == last));
                REQUIRE(chunkutil::IsOnChunkFaceBoundary(p, FaceDir::West)   == (x == 0));
            }
        }
    }
}

TEST_CASE("TryWrapCrossChunkLocal wraps only out-of-range coordinates") {
    SUBCASE("in-range positions are left alone") {
        int x = 5, y = 6, z = 7;
        FaceDir face{};
        CHECK_FALSE(chunkutil::TryWrapCrossChunkLocal(x, y, z, face));
        CHECK(x == 5);
        CHECK(y == 6);
        CHECK(z == 7);
    }

    SUBCASE("each axis wraps to the mirrored edge and reports its face") {
        struct Case { int x, y, z; int wx, wy, wz; FaceDir face; };
        const Case cases[] = {
            { -1,  5,  5,  15,  5,  5, FaceDir::West   },
            { 16,  5,  5,   0,  5,  5, FaceDir::East   },
            {  5, -1,  5,   5, 15,  5, FaceDir::Bottom },
            {  5, 16,  5,   5,  0,  5, FaceDir::Top    },
            {  5,  5, -1,   5,  5, 15, FaceDir::North  },
            {  5,  5, 16,   5,  5,  0, FaceDir::South  },
        };
        for (const Case& c : cases) {
            int x = c.x, y = c.y, z = c.z;
            FaceDir face{};
            REQUIRE(chunkutil::TryWrapCrossChunkLocal(x, y, z, face));
            CHECK(x == c.wx);
            CHECK(y == c.wy);
            CHECK(z == c.wz);
            CHECK(face == c.face);
        }
    }
}

} // TEST_SUITE("world.tier1.chunkutil")
