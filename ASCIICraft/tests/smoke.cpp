// Harness smoke tests.
//
// These exist to prove the test executable is wired up correctly, not to test
// game logic - real coverage starts in world/tier1_*.cpp. Specifically they
// prove that a test binary can link ASCIICraft_core and construct world types
// with no D3D11 device and no window.

#include <doctest/doctest.h>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

TEST_SUITE("smoke") {

TEST_CASE("harness runs") {
    CHECK(sizes::CHUNK_SIZE == 16);
    CHECK(sizes::REGION_SIZE == 32);
    CHECK(Chunk::VOLUME == 4096);
}

TEST_CASE("world types are constructible headless") {
    // Chunk pulls in ASCIIgL::Mesh and Camera3D. If the renderer were a hard
    // dependency rather than a link-time-only one, this would fault here.
    Chunk chunk(ChunkCoord{3, 7, -2});

    CHECK(chunk.GetCoord() == ChunkCoord{3, 7, -2});
    CHECK_FALSE(chunk.IsGenerated());
    CHECK(chunk.IsDirty());          // chunks start dirty, awaiting a first mesh

    SUBCASE("fresh chunk is entirely air") {
        bool allAir = true;
        for (int i = 0; i < Chunk::VOLUME; ++i) {
            if (chunk.GetBlockStateByIndex(i) != blockstate::BlockStateRegistry::AIR_STATE_ID) {
                allAir = false;
                break;
            }
        }
        CHECK(allAir);
    }

    SUBCASE("block writes read back") {
        chunk.SetBlockState(1, 2, 3, 42u);
        CHECK(chunk.GetBlockState(1, 2, 3) == 42u);
    }
}

TEST_CASE("coordinate conversions are reachable") {
    // A real round-trip lives in world/tier1_coords.cpp; this only proves the
    // header-only coord math links and runs.
    const WorldCoord w{-17, 40, 33};
    const ChunkCoord c = w.ToChunkCoord();
    const glm::ivec3 local = w.ToLocalChunkPos();

    CHECK(c == ChunkCoord{-2, 2, 2});
    CHECK(c.x * sizes::CHUNK_SIZE + local.x == w.x);
    CHECK(c.y * sizes::CHUNK_SIZE + local.y == w.y);
    CHECK(c.z * sizes::CHUNK_SIZE + local.z == w.z);
}

// The bug-pinning tests added in later phases rely on doctest inverting the
// result of a test marked should_fail: it reports PASSED while the assertion
// fails, and FAILED once the underlying bug is fixed. That inversion is what
// keeps `ctest` green while still forcing someone who lands a fix to notice.
//
// If doctest's semantics ever change, ~30 regression pins would silently flip
// meaning, so verify the mechanism itself rather than assuming it.
TEST_CASE("should_fail ratchet inverts the result" * doctest::should_fail()) {
    CHECK(false);
}

} // TEST_SUITE("smoke")
