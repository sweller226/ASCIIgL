#pragma once

#include <vector>
#include <cstdint>

#include <ASCIICraft/world/Coords.hpp>

/// Single block placement in world space (for trees and other cross-chunk features).
struct WorldBlockPlacement {
    WorldCoord pos;
    uint32_t stateId;
};

/// Result of generating terrain for one chunk. Used by the job queue and
/// TerrainGenerator; apply block data (and in future entities) on the main thread.
/// Extend with block entities (chests, furnaces), entities, etc. as needed.
struct TerrainResult {
    /// Block state IDs for the chunk (size Chunk::VOLUME when complete).
    std::vector<uint32_t> blocks;

    /// Blocks placed in world space (e.g. trees). Apply via ChunkManager::SetBlockState on the main thread.
    std::vector<WorldBlockPlacement> crossChunkBlocks;

    // Future: block entities (chests, furnaces, etc.)
    // std::vector<BlockEntityPlacement> block_entities;

    // Future: entities (mobs, items)
    // std::vector<EntityPlacement> entities;
};
