// Region-directory injection seam.
//
// RegionFile's constructor creates its directory as a side effect, so before this
// seam existed, merely constructing one wrote into the real save. These tests
// pin that a caller-supplied directory is honoured - the property every Tier 2 and
// Tier 3 test depends on for isolation.

#include <doctest/doctest.h>

#include "support/TempDir.hpp"

#include <ASCIICraft/save/SavePaths.hpp>
#include <ASCIICraft/world/chunk/ChunkManagerDeps.hpp>
#include <ASCIICraft/world/chunk/ChunkRegion.hpp>

#include <filesystem>

namespace fs = std::filesystem;

TEST_SUITE("world.tier1.regiondir") {

TEST_CASE("RegionFile creates its file in the supplied directory") {
    testsupport::TempDir dir("regionfile");
    const fs::path sub = dir / "nested";   // deliberately absent - must be created

    REQUIRE_FALSE(fs::exists(sub));
    {
        RegionFile region(RegionCoord{0, 0, 0}, sub);
        CHECK(fs::exists(sub));
    }

    // RegionFile does not open the file until first use, so only the directory is
    // guaranteed here. What matters is that it went to `sub`, not to the real save.
    CHECK(fs::is_directory(sub));
}

TEST_CASE("region filename encodes the coordinate, including negatives") {
    testsupport::TempDir dir("naming");

    // Force the file to exist by saving nothing through the batch API, which opens it.
    RegionFile region(RegionCoord{-1, 0, -1}, dir.Path());
    REQUIRE(region.BeginBatchSave());
    region.EndBatchSave();

    CHECK(fs::exists(dir / "r_-1.0.-1"));
}

TEST_CASE("RegionManager forwards its directory to every region it creates") {
    testsupport::TempDir dir("manager");
    RegionManager manager(dir.Path());

    CHECK(manager.GetRegionDir() == dir.Path());

    auto a = manager.GetOrCreate(RegionCoord{0, 0, 0});
    auto b = manager.GetOrCreate(RegionCoord{5, 0, -3});
    REQUIRE(a);
    REQUIRE(b);

    // Same coord returns the cached instance rather than a second file.
    CHECK(manager.GetOrCreate(RegionCoord{0, 0, 0}) == a);
}

TEST_CASE("two managers on separate directories do not share state") {
    // The isolation property: parallel tests must not observe each other's regions.
    testsupport::TempDir dirA("isolA");
    testsupport::TempDir dirB("isolB");

    RegionManager managerA(dirA.Path());
    RegionManager managerB(dirB.Path());

    auto a = managerA.GetOrCreate(RegionCoord{0, 0, 0});
    auto b = managerB.GetOrCreate(RegionCoord{0, 0, 0});
    REQUIRE(a);
    REQUIRE(b);

    CHECK(a != b);
    CHECK(dirA.Path() != dirB.Path());
}

TEST_CASE("ChunkManagerDeps defaults point at the real save layout") {
    // Constructed, not used: instantiating a RegionFile here would create world/regions
    // next to the test executable, which is exactly what this seam exists to avoid.
    ChunkManagerDeps deps;
    CHECK(deps.regionDir == save::RegionDir());
    CHECK(deps.regionDir == fs::path("world") / "regions");
    CHECK(deps.nowSeconds == nullptr);   // null => util::NowSeconds
    CHECK(deps.scheduler == nullptr);    // null => MakeTbbChunkJobScheduler
}

} // TEST_SUITE("world.tier1.regiondir")
