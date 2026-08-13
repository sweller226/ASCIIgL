// Backwards compatibility with v1 saves.
//
// v1 chunk and meta blobs stored RAW NUMERIC state ids, whose meaning depended on the
// block registration order at the time they were written. v2 stores names and
// properties instead. A world created before the v2 switch still contains v1 blobs,
// and reading them goes through v1_state_id::Remap.
//
// That path had no end-to-end coverage: the round-trip tests all write v2 and read it
// back, so they never exercise the migration at all. These tests construct genuine v1
// bytes by hand and read them with the current code.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>
#include <ASCIICraft/world/chunk/V1StateIdRemap.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// Legacy state ids, derived from v1_state_id::kV1TypeOrder and each type's state
/// count. The table is 130 entries:
///   air 0 | dandelion 1 | poppy 2 | tall_grass 3 | fern 4 | fence 5-20
///   oak_stairs 21-60 | cobblestone 61 | stone_stairs 62-101 | dirt 102
///   grass 103-104 | oak_log 105-108 | oak_planks 109 | oak_slab 110-111
///   cobblestone_slab 112-113 | oak_leaves 114 | crafting_table 115 | bookshelf 116
///   furnace 117-124 | glass 125 | blue_wool 126 | green_wool 127 | water 128-129
constexpr uint32_t kV1Air         = 0;
constexpr uint32_t kV1Cobblestone = 61;
constexpr uint32_t kV1Dirt        = 102;
constexpr uint32_t kV1Glass       = 125;

void Append(std::vector<uint8_t>& out, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

/// Builds a v1 chunk blob: header(version=1), palette header, raw numeric palette,
/// then 4-bit packed indices (low nibble first).
std::vector<uint8_t> BuildV1ChunkBlob(const std::vector<uint32_t>& palette,
                                      const std::vector<uint16_t>& indices) {
    REQUIRE(palette.size() <= 16);              // so indexBits is 4
    REQUIRE(indices.size() == Chunk::VOLUME);

    std::vector<uint8_t> blob;

    const ChunkHeader ch{ CHUNK_BLOB_VERSION_V1 };
    Append(blob, &ch, sizeof(ch));

    const PaletteHeader ph{ static_cast<uint16_t>(palette.size()), 4 };
    Append(blob, &ph, sizeof(ph));

    for (const uint32_t stateId : palette) {
        const SerializedBlock sb{ stateId };
        Append(blob, &sb, sizeof(sb));
    }

    for (size_t i = 0; i < indices.size(); i += 2) {
        const uint8_t low = static_cast<uint8_t>(indices[i] & 0x0F);
        const uint8_t high = (i + 1 < indices.size())
                           ? static_cast<uint8_t>(indices[i + 1] & 0x0F) : 0;
        blob.push_back(static_cast<uint8_t>(low | (high << 4)));
    }
    return blob;
}

/// Writes a region file containing exactly one chunk, whose blob is the supplied raw
/// bytes. Produces the same layout the old writer did: header, chunk index table,
/// meta index table, then blobs.
void WriteRegionWithRawBlob(const fs::path& file, const std::vector<uint8_t>& blob) {
    constexpr size_t kEntries = static_cast<size_t>(sizes::REGION_SIZE)
                              * sizes::REGION_SIZE * sizes::REGION_SIZE;
    const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
    const uint32_t chunkTable = static_cast<uint32_t>(kEntries * sizeof(ChunkIndexEntry));
    const uint32_t metaTable  = static_cast<uint32_t>(kEntries * sizeof(MetaBucketIndexEntry));
    const uint32_t blobStart  = headerSize + chunkTable + metaTable;

    RegionHeader header{};
    header.version = 1;              // what the old writer emitted
    header.chunkCount = 1;
    header.chunkStart = blobStart;
    header.metaStart  = blobStart;

    std::vector<ChunkIndexEntry> chunkIndexes(kEntries, ChunkIndexEntry{0, 0, 0});
    std::vector<MetaBucketIndexEntry> metaIndexes(kEntries, MetaBucketIndexEntry{0, 0, 0, 0});

    // Chunk (0,0,0) -> local (0,0,0) -> index 0.
    chunkIndexes[0].offset = blobStart;
    chunkIndexes[0].length = static_cast<uint32_t>(blob.size());
    chunkIndexes[0].flags  = 0x1;

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(chunkIndexes.data()),
              static_cast<std::streamsize>(chunkTable));
    out.write(reinterpret_cast<const char*>(metaIndexes.data()),
              static_cast<std::streamsize>(metaTable));
    out.write(reinterpret_cast<const char*>(blob.data()),
              static_cast<std::streamsize>(blob.size()));
}

} // namespace

TEST_SUITE("world.tier2.v1compat") {

TEST_CASE("the v1 remap table resolves legacy ids to the right blocks") {
    const auto& ids = testsupport::Ids();
    CHECK(v1_state_id::Remap(kV1Air) == ids.air);
    CHECK(v1_state_id::Remap(kV1Dirt) == ids.dirt);
    CHECK(v1_state_id::Remap(kV1Cobblestone) ==
          testsupport::SharedBlocks().GetDefaultState("minecraft:cobblestone"));
    CHECK(v1_state_id::Remap(kV1Glass) == ids.glass);

    // Out-of-range legacy ids degrade to air rather than indexing off the end.
    CHECK(v1_state_id::Remap(99999u) == ids.air);
}

TEST_CASE("a v1 chunk blob loads and remaps to current state ids") {
    testsupport::TempDir dir("v1_chunk");
    const auto& ids = testsupport::Ids();

    // Palette in legacy numbering; the loader must translate every entry.
    const std::vector<uint32_t> palette = { kV1Air, kV1Dirt, kV1Cobblestone, kV1Glass };

    std::vector<uint16_t> indices(Chunk::VOLUME, 0);
    indices[chunkutil::GetBlockIndex(0, 0, 0)] = 1;    // dirt
    indices[chunkutil::GetBlockIndex(5, 6, 7)] = 2;    // cobblestone
    indices[chunkutil::GetBlockIndex(15, 15, 15)] = 3; // glass

    WriteRegionWithRawBlob(dir / "r_0.0.0", BuildV1ChunkBlob(palette, indices));

    RegionFile region(RegionCoord{0, 0, 0}, dir.Path());
    Chunk loaded(ChunkCoord{0, 0, 0});
    REQUIRE(region.LoadChunk(&loaded, testsupport::SharedBlocks()));

    CHECK(loaded.GetBlockState(0, 0, 0) == ids.dirt);
    CHECK(loaded.GetBlockState(5, 6, 7) ==
          testsupport::SharedBlocks().GetDefaultState("minecraft:cobblestone"));
    CHECK(loaded.GetBlockState(15, 15, 15) == ids.glass);
    CHECK(loaded.GetBlockState(1, 1, 1) == ids.air);
}

TEST_CASE("a v1 world can be re-saved as v2 and still reads correctly") {
    // The realistic upgrade path: an old world loads, the player plays, the chunk is
    // written back in v2. Its contents must survive the format change.
    testsupport::TempDir dir("v1_upgrade");
    const auto& ids = testsupport::Ids();

    const std::vector<uint32_t> palette = { kV1Air, kV1Dirt };
    std::vector<uint16_t> indices(Chunk::VOLUME, 0);
    indices[chunkutil::GetBlockIndex(3, 3, 3)] = 1;
    WriteRegionWithRawBlob(dir / "r_0.0.0", BuildV1ChunkBlob(palette, indices));

    {
        RegionFile region(RegionCoord{0, 0, 0}, dir.Path());
        Chunk chunk(ChunkCoord{0, 0, 0});
        REQUIRE(region.LoadChunk(&chunk, testsupport::SharedBlocks()));
        REQUIRE(chunk.GetBlockState(3, 3, 3) == ids.dirt);

        // Play: mark generated (as ChunkManager does on a disk hit) and edit a block.
        chunk.SetGenerated(true);
        chunk.SetBlockState(4, 4, 4, ids.glass);
        REQUIRE(region.SaveChunk(&chunk, testsupport::SharedBlocks()));
    }

    RegionFile reopened(RegionCoord{0, 0, 0}, dir.Path());
    Chunk again(ChunkCoord{0, 0, 0});
    REQUIRE(reopened.LoadChunk(&again, testsupport::SharedBlocks()));
    CHECK(again.GetBlockState(3, 3, 3) == ids.dirt);    // migrated content preserved
    CHECK(again.GetBlockState(4, 4, 4) == ids.glass);   // new edit preserved
}

TEST_CASE("a region header written by the old code is accepted") {
    // The version check added for defect G must not reject legitimate old saves. The
    // old writer emitted version 1 and RegionHeader is byte-identical, so it should
    // load - this pins that, since getting it wrong would make every existing world
    // unopenable.
    testsupport::TempDir dir("v1_header");
    const std::vector<uint32_t> palette = { kV1Air, kV1Dirt };
    std::vector<uint16_t> indices(Chunk::VOLUME, 1);
    WriteRegionWithRawBlob(dir / "r_0.0.0", BuildV1ChunkBlob(palette, indices));

    RegionFile region(RegionCoord{0, 0, 0}, dir.Path());
    Chunk loaded(ChunkCoord{0, 0, 0});
    CHECK_NOTHROW((void)region.LoadChunk(&loaded, testsupport::SharedBlocks()));
}

} // TEST_SUITE("world.tier2.v1compat")
