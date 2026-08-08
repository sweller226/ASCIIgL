// Chunk mesh generation.
//
// BuildChunkMeshData is a pure function of raw block arrays plus two registries, so
// it is the easiest real seam in the world tree to test. The face-culling cases carry
// the weight: chunk-border culling is what decides whether the world looks solid or
// full of holes, and it depends on the neighbour-index / FaceDir mapping being right.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkMeshGen.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

#include <array>
#include <atomic>
#include <thread>
#include <vector>

namespace {

using Blocks = std::vector<uint32_t>;

Blocks Filled(uint32_t id) { return Blocks(Chunk::VOLUME, id); }

ChunkMeshData Build(const Blocks& chunk,
                    const std::array<const uint32_t*, 6>& neighbours) {
    return BuildChunkMeshData(ChunkCoord{0, 0, 0}, chunk.data(), neighbours,
                              &testsupport::SharedBlocks(), &testsupport::SharedModels());
}

std::array<const uint32_t*, 6> NoNeighbours() {
    return std::array<const uint32_t*, 6>{};   // all null
}

/// Quads, derived from the index count (6 indices per quad).
size_t QuadCount(const std::vector<int>& indices) { return indices.size() / 6; }

} // namespace

TEST_SUITE("world.tier1.meshgen") {

TEST_CASE("an all-air chunk produces no geometry") {
    const Blocks air = Filled(testsupport::Ids().air);
    const ChunkMeshData m = Build(air, NoNeighbours());

    CHECK_FALSE(m.HasOpaque());
    CHECK_FALSE(m.HasTransparent());
    CHECK_FALSE(m.HasOpaqueNoCull());
}

TEST_CASE("a single solid block produces exactly six faces") {
    Blocks blocks = Filled(testsupport::Ids().air);
    blocks[chunkutil::GetBlockIndex(8, 8, 8)] = testsupport::Ids().stone;

    const ChunkMeshData m = Build(blocks, NoNeighbours());
    CHECK(QuadCount(m.opaqueIndices) == 6);
}

TEST_CASE("interior faces of a solid chunk are culled") {
    // Only the 6 outer shells survive: 16*16 per face.
    const Blocks stone = Filled(testsupport::Ids().stone);
    const ChunkMeshData m = Build(stone, NoNeighbours());

    constexpr size_t kShell = sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
    CHECK(QuadCount(m.opaqueIndices) == 6 * kShell);
}

TEST_CASE("a solid chunk surrounded by solid neighbours produces nothing") {
    // The chunk-border culling test. If this regressed, every chunk boundary in the
    // world would render a redundant wall of faces.
    const Blocks stone = Filled(testsupport::Ids().stone);
    const Blocks neighbourData = Filled(testsupport::Ids().stone);

    std::array<const uint32_t*, 6> neighbours{};
    for (int i = 0; i < 6; ++i) neighbours[i] = neighbourData.data();

    const ChunkMeshData m = Build(stone, neighbours);
    CHECK(QuadCount(m.opaqueIndices) == 0);
}

TEST_CASE("each neighbour direction culls exactly one shell") {
    // Catches a transposed neighbour-index to FaceDir mapping, which would cull the
    // wrong side and leave holes on one axis while double-drawing another.
    const Blocks stone = Filled(testsupport::Ids().stone);
    const Blocks neighbourData = Filled(testsupport::Ids().stone);
    constexpr size_t kShell = sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;

    for (int dir = 0; dir < 6; ++dir) {
        std::array<const uint32_t*, 6> neighbours{};
        neighbours[dir] = neighbourData.data();

        const ChunkMeshData m = Build(stone, neighbours);
        CAPTURE(dir);
        REQUIRE(QuadCount(m.opaqueIndices) == 5 * kShell);
    }
}

TEST_CASE("a null neighbour is treated the same as an all-air neighbour") {
    // Matters at the render-distance edge, where a chunk legitimately has no loaded
    // neighbour. If these disagreed, borders would flicker as chunks stream in.
    const Blocks stone = Filled(testsupport::Ids().stone);
    const Blocks airNeighbour = Filled(testsupport::Ids().air);

    const ChunkMeshData withNull = Build(stone, NoNeighbours());

    std::array<const uint32_t*, 6> airNeighbours{};
    for (int i = 0; i < 6; ++i) airNeighbours[i] = airNeighbour.data();
    const ChunkMeshData withAir = Build(stone, airNeighbours);

    CHECK(QuadCount(withNull.opaqueIndices) == QuadCount(withAir.opaqueIndices));
    CHECK(withNull.opaqueVertices.size() == withAir.opaqueVertices.size());
}

TEST_CASE("water routes to the transparent buffer") {
    Blocks blocks = Filled(testsupport::Ids().air);
    blocks[chunkutil::GetBlockIndex(8, 8, 8)] = testsupport::Ids().water;

    const ChunkMeshData m = Build(blocks, NoNeighbours());
    CHECK(m.HasTransparent());
    CHECK_FALSE(m.HasOpaque());
}

TEST_CASE("cutout plants render as opaque geometry") {
    // Characterization, and it documents a latent gap.
    //
    // ChunkMeshGen routes to opaqueNoCull based on ResolvedBlockModel::opaqueNoCull,
    // but BlockModelResolver.cpp hardcodes that flag to false for every JSON-loaded
    // model - directly beneath a comment saying "Cross-like quads should render in the
    // opaque-no-cull bucket". So the no-cull bucket is currently unreachable and cross
    // plants go through the normal opaque path with backface culling applied.
    //
    // Not in scope for the chunk-lifecycle work, but if plants ever look one-sided,
    // this is why. Fixing the resolver flips this test, which is the intent.
    Blocks blocks = Filled(testsupport::Ids().air);
    blocks[chunkutil::GetBlockIndex(8, 8, 8)] = testsupport::Ids().poppy;

    const ChunkMeshData m = Build(blocks, NoNeighbours());
    CHECK(m.HasOpaque());
    CHECK_FALSE(m.HasTransparent());
    CHECK_FALSE(m.HasOpaqueNoCull());
}

TEST_CASE("a plant produces fewer faces than a full cube") {
    // Cross-shaped models are two intersecting quads, not six cube faces - a cheap
    // check that plants use their own geometry rather than falling back to a cube.
    Blocks plant = Filled(testsupport::Ids().air);
    plant[chunkutil::GetBlockIndex(8, 8, 8)] = testsupport::Ids().poppy;

    Blocks cube = Filled(testsupport::Ids().air);
    cube[chunkutil::GetBlockIndex(8, 8, 8)] = testsupport::Ids().stone;

    const size_t plantQuads = QuadCount(Build(plant, NoNeighbours()).opaqueIndices);
    const size_t cubeQuads  = QuadCount(Build(cube,  NoNeighbours()).opaqueIndices);

    CAPTURE(plantQuads);
    CAPTURE(cubeQuads);
    CHECK(plantQuads > 0);
    CHECK(plantQuads < cubeQuads);
}

TEST_CASE("mesh generation is deterministic") {
    Blocks blocks = Filled(testsupport::Ids().stone);
    blocks[chunkutil::GetBlockIndex(3, 4, 5)] = testsupport::Ids().air;
    blocks[chunkutil::GetBlockIndex(9, 2, 11)] = testsupport::Ids().water;

    const ChunkMeshData a = Build(blocks, NoNeighbours());
    const ChunkMeshData b = Build(blocks, NoNeighbours());

    CHECK(a.opaqueVertices == b.opaqueVertices);
    CHECK(a.opaqueIndices == b.opaqueIndices);
    CHECK(a.transparentVertices == b.transparentVertices);
}

TEST_CASE("mesh generation is thread-safe") {
    // Mesh jobs run concurrently on TBB workers against one shared registry and model
    // library. ChunkMeshGen also keeps a shared warn-once set behind a mutex, which
    // this exercises.
    Blocks blocks = Filled(testsupport::Ids().stone);
    blocks[chunkutil::GetBlockIndex(3, 4, 5)] = testsupport::Ids().air;

    const ChunkMeshData reference = Build(blocks, NoNeighbours());

    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 5; ++i) {
                const ChunkMeshData m = Build(blocks, NoNeighbours());
                if (m.opaqueVertices != reference.opaqueVertices ||
                    m.opaqueIndices != reference.opaqueIndices) {
                    mismatches.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    CHECK(mismatches.load() == 0);
}

} // TEST_SUITE("world.tier1.meshgen")
