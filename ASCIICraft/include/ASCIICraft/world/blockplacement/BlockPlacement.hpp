#pragma once

#include <cstdint>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <ASCIICraft/world/blockstate/BlockFace.hpp>
#include <ASCIICraft/world/blockplacement/GrassOrientation.hpp>

namespace ASCIICraft {

/// Context in which a block is being placed, determines placement behavior.
enum class PlacementContext {
    TerrainGeneration,    // World generation (deterministic, position-based)
    PlayerPlacement,      // Player placing blocks (could use look direction)
    StructureGeneration,  // Structure/feature placement (specific orientations)
    BlockUpdate           // Block update/neighbor change (neighbor-based)
};

/// Applies placement-time logic to a block state (e.g. grass orientation, directional blocks).
/// Call this before placing a block in the world, whether from terrain generation or player placement.
/// Returns the finalized stateId ready to be placed.
inline uint32_t GetFinalizedBlockStateForPlacement(
    blockstate::BlockStateRegistry& bsr,
    uint32_t stateId,
    const WorldCoord& position,
    PlacementContext context = PlacementContext::TerrainGeneration
) {
    // Get the block type to check if it needs placement-time adjustments
    uint16_t typeId = bsr.GetTypeIdFromState(stateId);
    const auto& type = bsr.GetType(typeId);
    
    // Grass blocks: apply orientation based on context
    if (type.name == "minecraft:grass") {
        BlockFace facing;
        
        switch (context) {
            case PlacementContext::TerrainGeneration:
                // Deterministic random orientation based on world position
                facing = GetGrassFacingForPosition(position);
                break;
                
            case PlacementContext::PlayerPlacement:
                // For now, same as terrain (position-based)
                // Future: could use player look direction
                facing = GetGrassFacingForPosition(position);
                break;
                
            case PlacementContext::StructureGeneration:
                // Default to north for structures (or could be configurable)
                facing = BlockFace::North;
                break;
                
            case PlacementContext::BlockUpdate:
                // Keep existing orientation (no change)
                return stateId;
        }
        
        stateId = bsr.WithProperty(stateId, "facing", BlockFaceToString(facing));
    }
    
    // Future: Add other block-specific placement logic here
    // e.g., logs orient based on adjacent logs, torches face walls, etc.
    
    return stateId;
}

/// Overload for individual coordinates
inline uint32_t GetFinalizedBlockStateForPlacement(
    blockstate::BlockStateRegistry& bsr,
    uint32_t stateId,
    int x, int y, int z,
    PlacementContext context = PlacementContext::TerrainGeneration
) {
    return GetFinalizedBlockStateForPlacement(bsr, stateId, WorldCoord(x, y, z), context);
}

} // namespace ASCIICraft
