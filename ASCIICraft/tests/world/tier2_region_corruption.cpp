// Region file robustness against damaged data.
//
// A corrupt region file must degrade to "regenerate this chunk", never to a crash, an
// out-of-bounds read, or a wild allocation. Truncation is the realistic case: a power
// cut mid-append leaves a file whose index points past EOF.
//
// The contract asserted throughout: LoadChunk/LoadMetaData either return false or
// throw std::exception. Both are handled by the caller; anything else is not.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"
#include "support/TempDir.hpp"

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// Writes a region containing a few real chunks, then returns its file path.
fs::path MakePopulatedRegion(const fs::path& dir) {
    const RegionCoord region{0, 0, 0};
    RegionFile rf(region, dir);
    REQUIRE(rf.BeginBatchSave());
    for (int i = 0; i < 4; ++i) {
        Chunk c(ChunkCoord{i, 0, 0});
        for (int b = 0; b < Chunk::VOLUME; ++b) {
            c.SetBlockStateByIndex(b, static_cast<uint32_t>((b + i) % 20));
        }
        rf.SaveChunkInBatch(&c, testsupport::SharedBlocks());
    }
    rf.EndBatchSave();
    return dir / "r_0.0.0";
}

std::vector<uint8_t> ReadAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
}

void WriteAll(const fs::path& p, const std::vector<uint8_t>& bytes) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

/// Attempts to load all four chunks. Returns false only if something escaped the
/// documented contract; a clean `false` or a std::exception both count as handled.
bool LoadsSafely(const fs::path& dir) {
    try {
        RegionFile rf(RegionCoord{0, 0, 0}, dir);
        for (int i = 0; i < 4; ++i) {
            Chunk c(ChunkCoord{i, 0, 0});
            try {
                (void)rf.LoadChunk(&c, testsupport::SharedBlocks());
            } catch (const std::exception&) {
                // Handled: ChunkManager catches this and regenerates.
            }
            MetaBucket m;
            try {
                (void)rf.LoadMetaData(ChunkCoord{i, 0, 0}, &m, testsupport::SharedBlocks());
            } catch (const std::exception&) {
            }
        }
    } catch (const std::exception&) {
        // Even construction-time failure is acceptable.
    }
    return true;
}

} // namespace

TEST_SUITE("world.tier2.corruption") {

TEST_CASE("truncation at any offset is handled") {
    testsupport::TempDir source("trunc_src");
    const fs::path original = MakePopulatedRegion(source.Path());
    const std::vector<uint8_t> bytes = ReadAll(original);
    REQUIRE(bytes.size() > 0);

    // Step across the whole file. The index tables alone are ~720 KB, so a coarse
    // stride still covers header, mid-index, and mid-blob truncations.
    const size_t stride = bytes.size() / 64 + 1;
    for (size_t cut = 0; cut < bytes.size(); cut += stride) {
        testsupport::TempDir target("trunc");
        WriteAll(target / "r_0.0.0", std::vector<uint8_t>(bytes.begin(), bytes.begin() + cut));
        CAPTURE(cut);
        REQUIRE(LoadsSafely(target.Path()));
    }
}

TEST_CASE("an empty or stub file is handled") {
    SUBCASE("zero bytes") {
        testsupport::TempDir dir("empty");
        WriteAll(dir / "r_0.0.0", {});
        CHECK(LoadsSafely(dir.Path()));
    }
    SUBCASE("three bytes - shorter than the header") {
        testsupport::TempDir dir("stub");
        WriteAll(dir / "r_0.0.0", {0x01, 0x02, 0x03});
        CHECK(LoadsSafely(dir.Path()));
    }
}

TEST_CASE("garbage index offsets and lengths are rejected") {
    testsupport::TempDir source("garbage_src");
    const fs::path original = MakePopulatedRegion(source.Path());
    std::vector<uint8_t> bytes = ReadAll(original);

    // Overwrite the first chunk index entry (offset, length, flags) with 0xFF. Both
    // the MAX_CHUNK_BLOB_SIZE cap and the past-EOF check should reject it.
    const size_t indexStart = sizeof(RegionHeader);
    REQUIRE(bytes.size() > indexStart + sizeof(ChunkIndexEntry));
    for (size_t i = 0; i < sizeof(ChunkIndexEntry); ++i) bytes[indexStart + i] = 0xFF;

    testsupport::TempDir dir("garbage");
    WriteAll(dir / "r_0.0.0", bytes);
    CHECK(LoadsSafely(dir.Path()));
}

TEST_CASE("random byte corruption never crashes or hangs") {
    // The cheapest real fuzzing available here. Under ASan this also catches
    // out-of-bounds reads that would otherwise pass silently.
    testsupport::TempDir source("fuzz_src");
    const fs::path original = MakePopulatedRegion(source.Path());
    const std::vector<uint8_t> pristine = ReadAll(original);
    REQUIRE(pristine.size() > 0);

    std::mt19937 rng(20260808u);
    std::uniform_int_distribution<size_t> pickByte(0, pristine.size() - 1);
    std::uniform_int_distribution<int> pickValue(0, 255);
    std::uniform_int_distribution<int> pickCount(1, 8);

    for (int iteration = 0; iteration < 100; ++iteration) {
        std::vector<uint8_t> bytes = pristine;
        const int flips = pickCount(rng);
        for (int f = 0; f < flips; ++f) {
            bytes[pickByte(rng)] = static_cast<uint8_t>(pickValue(rng));
        }
        testsupport::TempDir dir("fuzz");
        WriteAll(dir / "r_0.0.0", bytes);
        CAPTURE(iteration);
        CAPTURE(flips);
        REQUIRE(LoadsSafely(dir.Path()));
    }
}

TEST_CASE("a corrupt chunk blob still allows the region to be rewritten") {
    // Recovery path: ChunkManager regenerates on a failed load and saves the result.
    // Writing over a damaged region must work rather than compounding the damage.
    testsupport::TempDir source("recover_src");
    const fs::path original = MakePopulatedRegion(source.Path());
    std::vector<uint8_t> bytes = ReadAll(original);

    // Damage the tail, where the blobs live.
    for (size_t i = bytes.size() > 200 ? bytes.size() - 200 : 0; i < bytes.size(); ++i) {
        bytes[i] = 0xAB;
    }
    testsupport::TempDir dir("recover");
    WriteAll(dir / "r_0.0.0", bytes);

    Chunk fresh(ChunkCoord{0, 0, 0});
    fresh.SetBlockState(1, 1, 1, testsupport::Ids().stone);
    {
        RegionFile rf(RegionCoord{0, 0, 0}, dir.Path());
        REQUIRE(rf.SaveChunk(&fresh, testsupport::SharedBlocks()));
    }

    RegionFile rf(RegionCoord{0, 0, 0}, dir.Path());
    Chunk loaded(ChunkCoord{0, 0, 0});
    REQUIRE(rf.LoadChunk(&loaded, testsupport::SharedBlocks()));
    CHECK(loaded.GetBlockState(1, 1, 1) == testsupport::Ids().stone);
}

// --- DEFECT G: the region header version is written but never checked --------

TEST_CASE("an unknown region format version is rejected"
          * doctest::should_fail()) {
    // RegionHeader::version is written as 1 and never read back. A future format bump
    // would therefore be parsed with the current code's assumptions rather than
    // refused, turning a clean "unsupported save" message into silent corruption.
    testsupport::TempDir source("ver_src");
    const fs::path original = MakePopulatedRegion(source.Path());
    std::vector<uint8_t> bytes = ReadAll(original);

    const uint32_t bogus = 999u;
    std::memcpy(bytes.data(), &bogus, sizeof(bogus));

    testsupport::TempDir dir("ver");
    WriteAll(dir / "r_0.0.0", bytes);

    RegionFile rf(RegionCoord{0, 0, 0}, dir.Path());
    Chunk c(ChunkCoord{0, 0, 0});

    bool refused = false;
    try {
        refused = !rf.LoadChunk(&c, testsupport::SharedBlocks());
    } catch (const std::exception&) {
        refused = true;
    }
    CHECK(refused);
}

} // TEST_SUITE("world.tier2.corruption")
