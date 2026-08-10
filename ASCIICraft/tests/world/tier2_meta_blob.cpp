// Meta bucket (cross-chunk edit) serialization.
//
// Meta buckets carry blocks written into a chunk that was not loaded at the time -
// most importantly, the parts of a tree that spill across a chunk border. Losing them
// is the "trees cut off at chunk borders" symptom, so this file holds the first
// genuine bug pins:
//
//   D  two saves to the same chunk replace rather than merge
//   E  applied edits are never cleared, so they resurrect on every subsequent load

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>
#include <ASCIICraft/world/chunk/CrossChunkEdit.hpp>

#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

namespace {

CrossChunkEdit MakeEdit(int x, int y, int z, uint32_t stateId) {
    CrossChunkEdit e{};
    e.PackPos(x, y, z);
    e.stateId = stateId;
    return e;
}

/// (packedPos, stateId) pairs, order-independent, for comparing buckets.
std::set<std::pair<uint16_t, uint32_t>> AsSet(const MetaBucket& b) {
    std::set<std::pair<uint16_t, uint32_t>> out;
    for (const auto& e : b.edits) out.insert({e.packedPos, e.stateId});
    return out;
}

void SaveMeta(const std::filesystem::path& dir, const ChunkCoord& coord, const MetaBucket& bucket) {
    RegionFile region(coord.ToRegionCoord(), dir);
    REQUIRE(region.SaveMetaData(coord, &bucket, testsupport::SharedBlocks()));
}

MetaBucket LoadMeta(const std::filesystem::path& dir, const ChunkCoord& coord) {
    RegionFile region(coord.ToRegionCoord(), dir);
    MetaBucket out;
    region.LoadMetaData(coord, &out, testsupport::SharedBlocks());
    return out;
}

} // namespace

TEST_SUITE("world.tier2.metablob") {

TEST_CASE("a meta bucket round-trips") {
    testsupport::TempDir dir("meta_rt");
    const ChunkCoord coord{1, 2, 3};

    MetaBucket bucket;
    bucket.edits.push_back(MakeEdit(0, 0, 0, testsupport::Ids().stone));
    bucket.edits.push_back(MakeEdit(15, 15, 15, testsupport::Ids().oakLog));
    bucket.edits.push_back(MakeEdit(7, 3, 11, testsupport::Ids().poppy));

    SaveMeta(dir.Path(), coord, bucket);
    CHECK(AsSet(LoadMeta(dir.Path(), coord)) == AsSet(bucket));
}

TEST_CASE("a bucket holding every in-chunk position round-trips") {
    testsupport::TempDir dir("meta_full");
    const ChunkCoord coord{0, 0, 0};

    MetaBucket bucket;
    for (int z = 0; z < 16; ++z)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                bucket.edits.push_back(MakeEdit(x, y, z, testsupport::Ids().stone));
    REQUIRE(bucket.edits.size() == 4096);

    SaveMeta(dir.Path(), coord, bucket);
    CHECK(AsSet(LoadMeta(dir.Path(), coord)) == AsSet(bucket));
}

TEST_CASE("a two-edit bucket round-trips") {
    // Specifically exercises the v1/v2 disambiguation heuristic: a v1 blob begins with
    // a raw uint32 count, so a v1 bucket of exactly 2 edits has a leading dword equal
    // to META_BLOB_VERSION_V2. TryParseMetaBlobV2 must not be fooled either way.
    testsupport::TempDir dir("meta_two");
    const ChunkCoord coord{5, 5, 5};

    MetaBucket bucket;
    bucket.edits.push_back(MakeEdit(1, 1, 1, testsupport::Ids().dirt));
    bucket.edits.push_back(MakeEdit(2, 2, 2, testsupport::Ids().grass));

    SaveMeta(dir.Path(), coord, bucket);
    CHECK(AsSet(LoadMeta(dir.Path(), coord)) == AsSet(bucket));
}

TEST_CASE("an empty bucket is not written and loads as empty") {
    testsupport::TempDir dir("meta_empty");
    const ChunkCoord coord{3, 3, 3};

    MetaBucket empty;
    SaveMeta(dir.Path(), coord, empty);
    CHECK(LoadMeta(dir.Path(), coord).edits.empty());
}

TEST_CASE("loading meta for a chunk that has none reports empty") {
    testsupport::TempDir dir("meta_absent");
    RegionFile region(RegionCoord{0, 0, 0}, dir.Path());
    MetaBucket out;
    region.LoadMetaData(ChunkCoord{9, 9, 9}, &out, testsupport::SharedBlocks());
    CHECK(out.edits.empty());
}

TEST_CASE("different chunks in one region keep separate buckets") {
    // Control for the merge test below: per-chunk isolation already works, so a
    // failure there is specifically about same-chunk merging.
    testsupport::TempDir dir("meta_percoord");

    MetaBucket a;
    a.edits.push_back(MakeEdit(1, 1, 1, testsupport::Ids().stone));
    MetaBucket b;
    b.edits.push_back(MakeEdit(2, 2, 2, testsupport::Ids().glass));

    SaveMeta(dir.Path(), ChunkCoord{0, 0, 0}, a);
    SaveMeta(dir.Path(), ChunkCoord{1, 0, 0}, b);

    CHECK(AsSet(LoadMeta(dir.Path(), ChunkCoord{0, 0, 0})) == AsSet(a));
    CHECK(AsSet(LoadMeta(dir.Path(), ChunkCoord{1, 0, 0})) == AsSet(b));
}

// --- DEFECT D: meta blobs replace instead of merging -------------------------

TEST_CASE("two saves to the same chunk merge rather than replace") {
    // appendMetaBlobAndUpdateIndex overwrites the single index entry for the chunk, so
    // the second save orphans the first blob. Both sets of edits should survive.
    testsupport::TempDir dir("meta_merge");
    const ChunkCoord coord{2, 2, 2};

    MetaBucket first;
    first.edits.push_back(MakeEdit(1, 1, 1, testsupport::Ids().oakLog));
    SaveMeta(dir.Path(), coord, first);

    MetaBucket second;
    second.edits.push_back(MakeEdit(2, 2, 2, testsupport::Ids().oakLeaves));
    SaveMeta(dir.Path(), coord, second);

    const MetaBucket loaded = LoadMeta(dir.Path(), coord);
    CHECK(loaded.edits.size() == 2);
}

TEST_CASE("spills from two neighbours into one unloaded chunk both survive") {
    // The tree-cut-off scenario in serialization terms. Chunk A's tree spills into B,
    // then chunk C's tree spills into the same B. B is never loaded in between, so
    // each spill arrives as its own SaveMetaData - and the second erases the first.
    testsupport::TempDir dir("meta_twospill");
    const ChunkCoord target{1, 5, 1};

    MetaBucket fromA;
    fromA.edits.push_back(MakeEdit(0, 3, 0, testsupport::Ids().oakLog));
    fromA.edits.push_back(MakeEdit(0, 4, 0, testsupport::Ids().oakLeaves));
    SaveMeta(dir.Path(), target, fromA);

    MetaBucket fromC;
    fromC.edits.push_back(MakeEdit(15, 3, 15, testsupport::Ids().oakLog));
    fromC.edits.push_back(MakeEdit(15, 4, 15, testsupport::Ids().oakLeaves));
    SaveMeta(dir.Path(), target, fromC);

    const MetaBucket loaded = LoadMeta(dir.Path(), target);
    const auto got = AsSet(loaded);

    bool hasA = false, hasC = false;
    for (const auto& e : fromA.edits) if (got.count({e.packedPos, e.stateId})) hasA = true;
    for (const auto& e : fromC.edits) if (got.count({e.packedPos, e.stateId})) hasC = true;
    CHECK((hasA && hasC));
}

// --- DEFECT E: applied edits are never cleared -------------------------------

TEST_CASE("meta edits are cleared once they have been applied to a chunk") {
    // Meta index flags are only ever OR'd with 0x1, never cleared, so a bucket stays
    // on disk forever. Every subsequent load re-applies it on top of the chunk blob.
    //
    // Concretely: a tree spills a log into chunk B. B loads, applies it, and saves it
    // as part of its own blob. The player then mines that log and B is saved again.
    // On the next load the stale meta puts the log straight back.
    testsupport::TempDir dir("meta_stale");
    const ChunkCoord coord{4, 4, 4};

    MetaBucket spill;
    spill.edits.push_back(MakeEdit(8, 8, 8, testsupport::Ids().oakLog));
    SaveMeta(dir.Path(), coord, spill);

    // The chunk loads, applies the edit, and later persists its own state - with the
    // block mined back out to air.
    Chunk chunk(coord);
    chunk.SetBlockState(8, 8, 8, testsupport::Ids().air);
    chunk.SetGenerated(true);   // the save path only persists generated chunks
    {
        RegionFile region(coord.ToRegionCoord(), dir.Path());
        REQUIRE(region.SaveChunk(&chunk, testsupport::SharedBlocks()));
    }

    // Reload the way ChunkManager does: chunk blob first, then meta on top.
    Chunk reloaded(coord);
    RegionFile region(coord.ToRegionCoord(), dir.Path());
    REQUIRE(region.LoadChunk(&reloaded, testsupport::SharedBlocks()));

    MetaBucket residual;
    region.LoadMetaData(coord, &residual, testsupport::SharedBlocks());
    for (const auto& e : residual.edits) {
        int x = 0, y = 0, z = 0;
        e.UnpackPos(x, y, z);
        reloaded.SetBlockState(x, y, z, e.stateId);
    }

    CHECK(reloaded.GetBlockState(8, 8, 8) == testsupport::Ids().air);
}

} // TEST_SUITE("world.tier2.metablob")
