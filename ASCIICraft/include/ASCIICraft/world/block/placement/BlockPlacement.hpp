#pragma once

#include <cstdint>
#include <optional>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

class ChunkManager;

namespace blockplacement {

/// Context in which a block is being placed, determines placement behavior.
enum class PlacementContext {
    TerrainGeneration,    // World generation (deterministic, position-based)
    PlayerPlacement,      // Player placing blocks (could use look direction)
    StructureGeneration,  // Structure/feature placement (specific orientations)
    BlockUpdate           // Block update/neighbor change (neighbor-based)
};

/// Applies placement-time logic that depends on neighbor state.
uint32_t FinalizePlacedState(
    const blockstate::BlockStateRegistry& bsr,
    const ChunkManager& chunkManager,
    uint32_t stateId,
    const WorldCoord& position,
    PlacementContext context = PlacementContext::TerrainGeneration,
    const bool keepStateId = false,
    std::optional<FaceDir> faceDir = std::nullopt
);

/// Overload for individual coordinates.
uint32_t FinalizePlacedState(
    const blockstate::BlockStateRegistry& bsr,
    const ChunkManager& chunkManager,
    uint32_t stateId,
    int x, int y, int z,
    PlacementContext context = PlacementContext::TerrainGeneration,
    const bool keepStateId = false,
    std::optional<FaceDir> faceDir = std::nullopt
);

/// Applies placement-time logic that does not require neighbor queries.
uint32_t FinalizePlacedStateBasic(
    const blockstate::BlockStateRegistry& bsr,
    uint32_t stateId,
    const WorldCoord& position,
    PlacementContext context = PlacementContext::TerrainGeneration,
    const bool keepStateId = false,
    std::optional<FaceDir> faceDir = std::nullopt
);

/// Overload for individual coordinates.
uint32_t FinalizePlacedStateBasic(
    const blockstate::BlockStateRegistry& bsr,
    uint32_t stateId,
    int x, int y, int z,
    PlacementContext context = PlacementContext::TerrainGeneration,
    const bool keepStateId = false,
    std::optional<FaceDir> faceDir = std::nullopt
);

}