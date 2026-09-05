#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

// Full definition, not a forward declaration: the unique_ptr member below needs a
// complete type to instantiate its deleter wherever ChunkManagerDeps is destroyed.
#include <ASCIICraft/world/chunk/IChunkJobScheduler.hpp>

#include <ASCIICraft/save/SavePaths.hpp>

/// Injectable collaborators for ChunkManager.
///
/// Every field defaults to the behaviour ChunkManager had before this struct existed,
/// so `ChunkManager(registry, dims, renderDistance, seed)` is unchanged. Tests
/// override them to get isolation (a temp region directory), reachable timeouts
/// (a fake clock), and deterministic job ordering (a manual scheduler).
struct ChunkManagerDeps {
    /// Directory holding region files. Relative paths resolve against the process
    /// CWD, which is what the game has always done.
    ///
    /// This is not merely cosmetic for tests: RegionFile's constructor calls
    /// create_directories() on this path, so leaving it at the default - the real
    /// save at `world/regions` - would have every test writing into and reading
    /// from the player's world.
    std::filesystem::path regionDir = save::RegionDir();

    /// Monotonic seconds, used for meta-bucket expiry and autosave. Null uses
    /// util::NowSeconds. A fake clock turns the 300s bucket timeout and 60s autosave
    /// into something a test can reach without actually waiting.
    std::function<uint32_t()> nowSeconds = nullptr;

    /// Job execution strategy. Null uses MakeTbbChunkJobScheduler().
    std::unique_ptr<IChunkJobScheduler> scheduler = nullptr;
};
