#pragma once

#include <vector>
#include <cstdint>

#include <ASCIICraft/world/Coords.hpp>

/// Single block placement in world space (for trees and other cross-chunk features).
struct WorldBlockPlacement {
    WorldCoord pos;
    uint32_t stateId;
};

/// Result of generating terrain for one chunk. Terrain block data is written
/// directly into the chunk by the worker; only cross-chunk data (e.g. trees) is
/// returned for main-thread application via ChunkManager::SetBlockState.
struct TerrainResult {
    /// Blocks placed in world space (e.g. trees). Apply via ChunkManager::SetBlockState on the main thread.
    std::vector<WorldBlockPlacement> crossChunkBlocks;

    // Future: block entities (chests, furnaces, etc.)
    // std::vector<BlockEntityPlacement> block_entities;

    // Future: entities (mobs, items)
    // std::vector<EntityPlacement> entities;
};
