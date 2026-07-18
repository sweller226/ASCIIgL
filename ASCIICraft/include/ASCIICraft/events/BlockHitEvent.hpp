#pragma once

#include <cstdint>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

namespace events {

/// Emitted periodically by MiningSystem while a block is being mined (survival).
/// ParticleSystem consumes it to spawn small crack puffs on the targeted face.
struct BlockHitEvent {
    WorldCoord position;
    uint32_t stateId = 0;
    FaceDir face = FaceDir::Top;  // approximate face toward the player
};

}
