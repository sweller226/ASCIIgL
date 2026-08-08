// ChunkManager inspection API, and the headless-construction property it rests on.
//
// Constructing a ChunkManager reaches Renderer::GetInst().GetBackgroundCol(). That is
// a plain field read on a default-constructed Impl with no D3D11 device, but the whole
// Tier 3 plan depends on it, so prove it rather than assume it.

#include <doctest/doctest.h>

#include "support/TempDir.hpp"

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/block/VanillaBlockRegistration.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>
#include <ASCIICraft/world/chunk/ChunkManagerDeps.hpp>

#include <entt/entt.hpp>

#include <memory>

namespace {

/// A registry with vanilla blocks registered, built once per process.
///
/// Registration reads ~40 JSON blockstates plus their models, so doing it per test
/// would dominate runtime. It also builds the process-global v1 remap table, which
/// only one registry may own - sharing one instance keeps that unambiguous.
entt::registry& SharedRegistry() {
    static entt::registry registry = [] {
        entt::registry r;
        blockstate::RegisterVanillaBlocksInContext(r);
        return r;
    }();
    return registry;
}

const sizes::WorldDimensions kDims(1024, 0, 1024);

/// A ChunkManager rooted at an isolated temp directory. No chunks are loaded:
/// streaming needs a player entity, which arrives with the Tier 3 harness.
std::unique_ptr<ChunkManager> MakeManager(const testsupport::TempDir& dir) {
    ChunkManagerDeps deps;
    deps.regionDir = dir.Path();
    return std::make_unique<ChunkManager>(SharedRegistry(), kDims, /*renderDistance=*/1,
                                          /*worldSeed=*/12345ULL, std::move(deps));
}

} // namespace

TEST_SUITE("world.tier1.chunkmanager") {

TEST_CASE("ChunkManager constructs and destructs headless") {
    testsupport::TempDir dir("cm_ctor");
    auto manager = MakeManager(dir);
    REQUIRE(manager != nullptr);
    CHECK(manager->GetRenderDistance() == 1);
    CHECK(manager->GetChunkLoadDistance() == 2);   // renderDistance + 1
}

TEST_CASE("a fresh manager reports empty stats") {
    testsupport::TempDir dir("cm_stats");
    auto manager = MakeManager(dir);

    const ChunkManager::Stats s = manager->GetStats();
    CHECK(s.loadedChunks == 0);
    CHECK(s.generatedChunks == 0);
    CHECK(s.pendingCrossChunkBuckets == 0);
    CHECK(s.metaTimeTrackerSize == 0);
    CHECK(manager->GetLoadedCoords().empty());
}

TEST_CASE("chunk accessors report absence rather than fabricating a chunk") {
    testsupport::TempDir dir("cm_absent");
    auto manager = MakeManager(dir);

    CHECK(manager->GetChunkShared(ChunkCoord{0, 0, 0}) == nullptr);
    CHECK(manager->GetChunkShared(ChunkCoord{-9, 4, 17}) == nullptr);
    CHECK_FALSE(manager->HasPendingCrossChunkEdits(ChunkCoord{0, 0, 0}));
    CHECK(manager->GetPendingCrossChunkEdits(ChunkCoord{0, 0, 0}).empty());
}

TEST_CASE("SetBlockState on an unloaded chunk buffers a cross-chunk edit") {
    // Exercises the accessors against real state, and pins the buffering behaviour
    // the tree-spill bugs hinge on: a write to a chunk that is not loaded must be
    // retained, not dropped.
    testsupport::TempDir dir("cm_edits");
    auto manager = MakeManager(dir);

    const ChunkCoord target{40, 5, 40};   // far outside the load radius
    manager->SetBlockState(target.x * 16 + 3, target.y * 16 + 4, target.z * 16 + 5, 7u);

    CHECK(manager->HasPendingCrossChunkEdits(target));

    const auto edits = manager->GetPendingCrossChunkEdits(target);
    REQUIRE(edits.size() == 1);
    CHECK(edits[0].stateId == 7u);

    int x = 0, y = 0, z = 0;
    edits[0].UnpackPos(x, y, z);
    CHECK(x == 3);
    CHECK(y == 4);
    CHECK(z == 5);

    const ChunkManager::Stats s = manager->GetStats();
    CHECK(s.pendingCrossChunkBuckets == 1);
    CHECK(s.metaTimeTrackerSize == 1);   // enrolled for expiry
}

TEST_CASE("FlushExpiredMetaBuckets(force) drains buckets a timed flush would keep") {
    testsupport::TempDir dir("cm_flush");
    auto manager = MakeManager(dir);

    const ChunkCoord target{40, 5, 40};
    manager->SetBlockState(target.x * 16 + 1, target.y * 16 + 2, target.z * 16 + 3, 9u);
    REQUIRE(manager->GetStats().pendingCrossChunkBuckets == 1);

    SUBCASE("unforced leaves a fresh bucket alone") {
        manager->FlushExpiredMetaBuckets(/*force=*/false);
        // lastTouched is recent, so META_BUCKET_TIME_LIMIT has not elapsed.
        CHECK(manager->GetStats().pendingCrossChunkBuckets == 1);
    }

    SUBCASE("forced persists and drops it") {
        manager->FlushExpiredMetaBuckets(/*force=*/true);
        CHECK(manager->GetStats().pendingCrossChunkBuckets == 0);
        CHECK_FALSE(manager->HasPendingCrossChunkEdits(target));
    }
}

} // TEST_SUITE("world.tier1.chunkmanager")
