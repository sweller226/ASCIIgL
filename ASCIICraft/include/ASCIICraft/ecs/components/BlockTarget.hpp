#pragma once

#include <cstdint>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace ecs::components {

/// Block the player is currently looking at within reach (updated by BlockTargetSystem).
struct BlockTarget {
    bool active = false;
    WorldCoord blockPos{};
    uint32_t stateId = blockstate::BlockStateRegistry::AIR_STATE_ID;

    /// Adjacent empty cell where a block would be placed (valid when canPlace is true).
    bool canPlace = false;
    WorldCoord placePos{};
};

} // namespace ecs::components
