#include <ASCIICraft/world/query/BlockQueries.hpp>

#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace blockquery {

bool IsAir(uint32_t stateId) {
    return stateId == blockstate::BlockStateRegistry::AIR_STATE_ID;
}

bool IsNonAir(uint32_t stateId) {
    return !IsAir(stateId);
}

bool TryGetState(
    const blockstate::BlockStateRegistry *bsr,
    uint32_t stateId,
    const blockstate::BlockState *&out
) {
    if (!bsr || !bsr->IsValidState(stateId)) {
        return false;
    }
    out = &bsr->GetState(stateId);
    return true;
}

bool IsSolidForPhysics(const blockstate::BlockStateRegistry *bsr, uint32_t stateId) {
    if (IsAir(stateId)) {
        return false;
    }
    const blockstate::BlockState *state = nullptr;
    if (TryGetState(bsr, stateId, state)) {
        return state->isFullBlock;
    }
    // Registry missing or invalid id: behave like legacy “any non-air is solid”.
    return true;
}

BlockFilter SolidForPhysicsFilter(const blockstate::BlockStateRegistry *bsr) {
    return [bsr](uint32_t stateId) { return IsSolidForPhysics(bsr, stateId); };
}

} // namespace blockquery
