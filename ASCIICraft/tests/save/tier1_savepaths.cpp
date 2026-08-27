// Save layout and the one-shot legacy migration.
//
// The migration runs once, on startup, before anything constructs a RegionFile. It
// moves a pre-`world/` save (`<cwd>/regions`) into `world/regions`. Its whole job is
// to be transparent when it works and to never destroy a save when it cannot - so
// the interesting cases here are the refusals, not the happy path.

#include <doctest/doctest.h>

#include "support/TempDir.hpp"

#include <ASCIICraft/save/SavePaths.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

/// Writes a file with known contents so a move can be told apart from a fresh
/// directory that merely has the right name.
void WriteSentinel(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

bool SentinelReads(const fs::path& path, const std::string& expected) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::string got;
    std::getline(in, got);
    return got == expected;
}

} // namespace

TEST_SUITE("save.tier1.savepaths") {

TEST_CASE("paths compose from the world root") {
    CHECK(save::WorldRoot() == fs::path("world"));
    CHECK(save::RegionDir() == fs::path("world") / "regions");
    CHECK(save::PlayerDataFile() == fs::path("world") / "player_data.json");
}

TEST_CASE("migration moves a legacy save under the world root") {
    testsupport::TempDir dir("migrate");
    const fs::path root   = dir / "world";
    const fs::path legacy = dir / "regions";

    WriteSentinel(legacy / "r_0.0.0", "chunk-blob");

    CHECK(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");

    // The data moved rather than being recreated empty.
    CHECK(SentinelReads(root / "regions" / "r_0.0.0", "chunk-blob"));
    CHECK_FALSE(fs::exists(legacy));
}

TEST_CASE("migration is a no-op when there is no legacy save") {
    testsupport::TempDir dir("nolegacy");
    const fs::path root   = dir / "world";
    const fs::path legacy = dir / "regions";

    CHECK(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");

    // Notably it does NOT create the root - that is RegionFile's job, and creating it
    // here would be indistinguishable from a migration that had already run.
    CHECK_FALSE(fs::exists(root));
}

TEST_CASE("migration refuses when both layouts exist and destroys neither") {
    testsupport::TempDir dir("conflict");
    const fs::path root   = dir / "world";
    const fs::path legacy = dir / "regions";

    WriteSentinel(root / "regions" / "r_0.0.0", "current-save");
    WriteSentinel(legacy / "r_0.0.0", "legacy-save");

    // The already-migrated save wins; the legacy directory is left for the player.
    CHECK(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");

    // Both survive untouched. Picking one would silently discard a world.
    CHECK(SentinelReads(root / "regions" / "r_0.0.0", "current-save"));
    CHECK(SentinelReads(legacy / "r_0.0.0", "legacy-save"));
}

TEST_CASE("migration is idempotent") {
    testsupport::TempDir dir("idempotent");
    const fs::path root   = dir / "world";
    const fs::path legacy = dir / "regions";

    WriteSentinel(legacy / "r_0.0.0", "chunk-blob");

    REQUIRE(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");

    // Second launch: nothing left to move, and the migrated save is left alone.
    CHECK(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");
    CHECK(SentinelReads(root / "regions" / "r_0.0.0", "chunk-blob"));
}

TEST_CASE("a legacy path that is a regular file is ignored") {
    testsupport::TempDir dir("legacyfile");
    const fs::path root   = dir / "world";
    const fs::path legacy = dir / "regions";

    WriteSentinel(legacy, "not-a-directory");

    CHECK(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");
    CHECK(fs::is_regular_file(legacy));
    CHECK_FALSE(fs::exists(root));
}

TEST_CASE("a blocked migration falls back to the legacy save rather than a new world") {
    // Regression, observed for real: an open handle inside the legacy directory makes
    // fs::rename fail on Windows. The first cut of this code then carried on with
    // world/regions anyway, so the player was dropped into a freshly generated empty
    // world - and because world/ now existed, the "both layouts present" guard refused
    // to migrate on any later launch, stranding the real save for good.
    testsupport::TempDir dir("blocked");
    const fs::path root   = dir / "world";
    const fs::path legacy = dir / "regions";

    WriteSentinel(legacy / "r_0.0.0", "chunk-blob");

    // Hold the region file open for writing. Windows refuses to move a directory
    // containing an open handle, which is exactly the real-world failure.
    std::ofstream hold(legacy / "r_0.0.0", std::ios::binary | std::ios::app);
    REQUIRE(hold.is_open());

    const fs::path chosen = save::MigrateLegacySaveLayout(root, legacy);

    hold.close();

    // The player keeps their own world this session rather than a generated one.
    CHECK(chosen == legacy);
    CHECK(SentinelReads(legacy / "r_0.0.0", "chunk-blob"));

    // And the half-made root must not be left behind: it would trip the
    // "world root already exists" guard and block the retry on every future launch.
    CHECK_FALSE(fs::exists(root));

    // With the handle released, the next launch migrates cleanly.
    CHECK(save::MigrateLegacySaveLayout(root, legacy) == root / "regions");
    CHECK(SentinelReads(root / "regions" / "r_0.0.0", "chunk-blob"));
}

TEST_CASE("an uncreatable world root falls back to the legacy save") {
    // Same contract as above via the other failure branch, and fully deterministic:
    // create_directories cannot make a directory underneath a regular file.
    testsupport::TempDir dir("badroot");
    const fs::path blocker = dir / "blocker";
    const fs::path root    = blocker / "world";
    const fs::path legacy  = dir / "regions";

    WriteSentinel(blocker, "i-am-a-file");
    WriteSentinel(legacy / "r_0.0.0", "chunk-blob");

    CHECK(save::MigrateLegacySaveLayout(root, legacy) == legacy);
    CHECK(SentinelReads(legacy / "r_0.0.0", "chunk-blob"));
}

} // TEST_SUITE("save.tier1.savepaths")
