// Tree generation seam.
//
// Reachability and determinism only - Phase 2 extends this file with the geometry
// assertions (contiguous trunk, leaf/log overlap, negative-coordinate leaf counts).
// What matters here is that lifting GenerateTree out of the anonymous namespace left
// dispatch intact and that it is callable without a TerrainGenerator.

#include <doctest/doctest.h>

#include <ASCIICraft/world/block/VanillaBlockRegistration.hpp>
#include <ASCIICraft/world/terrain/TreeGen.hpp>

#include <entt/entt.hpp>

#include <set>
#include <tuple>

namespace {

const blockstate::BlockStateRegistry& Blocks() {
    static entt::registry registry = [] {
        entt::registry r;
        blockstate::RegisterVanillaBlocksInContext(r);
        return r;
    }();
    return registry.ctx().get<blockstate::BlockStateRegistry>();
}

} // namespace

TEST_SUITE("world.tier1.trees") {

TEST_CASE("GenerateTree emits blocks without a TerrainGenerator") {
    std::vector<WorldBlockPlacement> out;
    terrain::GenerateTree(BiomeType::Forest, 100, 80, 100, Blocks(), out);

    CHECK_FALSE(out.empty());

    SUBCASE("output contains both logs and leaves") {
        const uint32_t logId    = Blocks().GetDefaultState("minecraft:oak_log");
        const uint32_t leavesId = Blocks().GetDefaultState("minecraft:oak_leaves");

        int logs = 0, leaves = 0;
        for (const auto& p : out) {
            if (p.stateId == logId) ++logs;
            if (p.stateId == leavesId) ++leaves;
        }
        // Log count uses the default (axis=y) state only; trunks are axis=y, branches
        // are axis=x/z, so this is a lower bound rather than the full trunk.
        CHECK(logs > 0);
        CHECK(leaves > 0);
    }
}

TEST_CASE("GenerateTree is deterministic for a given position") {
    std::vector<WorldBlockPlacement> a, b;
    terrain::GenerateTree(BiomeType::Forest, 100, 80, 100, Blocks(), a);
    terrain::GenerateTree(BiomeType::Forest, 100, 80, 100, Blocks(), b);

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].pos == b[i].pos);
        CHECK(a[i].stateId == b[i].stateId);
    }
}

TEST_CASE("both biomes dispatch to a tree") {
    std::vector<WorldBlockPlacement> forest, flower;
    terrain::GenerateTree(BiomeType::Forest,       200, 80, 200, Blocks(), forest);
    terrain::GenerateTree(BiomeType::FlowerForest, 200, 80, 200, Blocks(), flower);

    CHECK_FALSE(forest.empty());
    CHECK_FALSE(flower.empty());
    // Same position, same roll, different species tables - the shapes must differ,
    // which is what proves the biome switch survived the move out of the namespace.
    CHECK(forest.size() != flower.size());
}

TEST_CASE("a tree near a chunk edge emits blocks in more than one chunk") {
    // Precondition for the Tier 3 spill tests: the generator really does produce
    // out-of-chunk positions, so a cut-off tree is the manager's fault, not this.
    std::vector<WorldBlockPlacement> out;
    terrain::GenerateTree(BiomeType::Forest, 15, 80, 15, Blocks(), out);
    REQUIRE_FALSE(out.empty());

    std::set<std::tuple<int, int, int>> chunks;
    for (const auto& p : out) {
        const ChunkCoord c = p.pos.ToChunkCoord();
        chunks.insert({c.x, c.y, c.z});
    }
    CHECK(chunks.size() > 1);
}

} // TEST_SUITE("world.tier1.trees")
