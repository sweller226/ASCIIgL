#pragma once

#include <cstdint>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace ecs::components {

/// The single in-flight timed mining operation (updated by MiningSystem, read
/// by the crack overlay renderer). Only one block can be mined at a time;
/// progress is discarded when the target changes or the button is released,
/// matching vanilla Minecraft.
struct MiningProgress {
    bool active = false;
    WorldCoord blockPos{};
    uint32_t stateId = blockstate::BlockStateRegistry::AIR_STATE_ID;
    float progress = 0.0f;       // 0..1, block breaks at >= 1
    float breakCooldown = 0.0f;  // seconds until mining can resume after a break
};

} // namespace ecs::components
