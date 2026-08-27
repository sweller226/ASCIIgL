#pragma once

#include <filesystem>

/// On-disk save layout.
///
///     world/
///       regions/
///         r_<x>.<y>.<z>      chunk + metadata blobs (binary, see ChunkRegion.hpp)
///       player_data.json     position, camera yaw/pitch, game mode
///
/// Before this layout existed, region files sat directly in `<cwd>/regions`. A save in
/// that shape is upgraded once, transparently, by MigrateLegacySaveLayout().
namespace save {

inline constexpr const char* kWorldRootName       = "world";
inline constexpr const char* kRegionsSubdirName   = "regions";
inline constexpr const char* kPlayerDataFileName  = "player_data.json";
/// Pre-`world/` layout: region files lived directly in `<cwd>/regions`.
inline constexpr const char* kLegacyRegionDirName = "regions";

// Relative paths resolve against the process CWD, which is what the game has always done.
inline std::filesystem::path WorldRoot()      { return kWorldRootName; }
inline std::filesystem::path RegionDir()      { return WorldRoot() / kRegionsSubdirName; }
inline std::filesystem::path PlayerDataFile() { return WorldRoot() / kPlayerDataFileName; }

/// One-shot upgrade of a pre-`world/` save, and the authority on which region
/// directory to open this session.
///
/// Moves \p legacyRegionDir to `<worldRoot>/regions` iff the legacy directory exists and
/// the world root does not. Never merges and never overwrites: if both are present it
/// logs and leaves them both alone. Never throws.
///
/// Must run before anything constructs a RegionFile - that constructor creates its
/// directory as a side effect, which would materialise the world root and make this a
/// no-op forever after.
///
/// \return the region directory the caller must actually use. Normally
///         `<worldRoot>/regions`. If a legacy save exists but could NOT be moved - a
///         file handle held open, antivirus, a cross-device root - this returns the
///         legacy directory instead, so the player still loads their own world and the
///         migration simply retries next launch. Returning the world root there would
///         hand them a freshly generated empty world AND strand the old save behind the
///         "world root already exists" guard permanently.
///
/// Both path parameters are defaulted purely so tests can drive this inside a TempDir
/// without mutating the process CWD.
std::filesystem::path MigrateLegacySaveLayout(
    const std::filesystem::path& worldRoot = WorldRoot(),
    const std::filesystem::path& legacyRegionDir = kLegacyRegionDirName);

} // namespace save
