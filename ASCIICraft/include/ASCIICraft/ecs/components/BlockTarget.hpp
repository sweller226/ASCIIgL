#pragma once

#include <cstdint>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

namespace ecs::components {

/// Block the player is currently looking at within reach (updated by BlockTargetSystem).
struct BlockTarget {
    bool active = false;
    WorldCoord blockPos{};
    uint32_t stateId = blockstate::BlockStateRegistry::AIR_STATE_ID;
    /// Face of the selection AABB that the look ray entered.
    FaceDir hitFace = FaceDir::North;
    /// Y of the hit point within the targeted cell [0, 1] (used for slab/stair half selection).
    float hitLocalY = 0.0f;

    /// Adjacent empty cell where a block would be placed (valid when canPlace is true).
    bool canPlace = false;
    WorldCoord placePos{};
};

} // namespace ecs::components
