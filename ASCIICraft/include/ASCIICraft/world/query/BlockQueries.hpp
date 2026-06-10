#pragma once

#include <cstdint>
#include <functional>

namespace blockstate {
class BlockStateRegistry;
struct BlockState;
} // namespace blockstate

namespace blockquery {

using BlockFilter = std::function<bool(uint32_t stateId)>;

bool IsAir(uint32_t stateId);
bool IsNonAir(uint32_t stateId);

bool TryGetState(
    const blockstate::BlockStateRegistry *bsr,
    uint32_t stateId,
    const blockstate::BlockState *&out
);

/// True if this state should participate in AABB physics collision (full-block models only).
bool IsSolidForPhysics(const blockstate::BlockStateRegistry *bsr, uint32_t stateId);

BlockFilter SolidForPhysicsFilter(const blockstate::BlockStateRegistry *bsr);

} // namespace blockquery
