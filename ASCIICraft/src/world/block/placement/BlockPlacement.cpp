#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>
#include <ASCIICraft/world/block/placement/FencePlacement.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>

namespace blockplacement {

    uint32_t FinalizePlacedState(
        const blockstate::BlockStateRegistry& bsr,
        const ChunkManager& chunkManager,
        uint32_t stateId,
        const WorldCoord& position,
        PlacementContext context,
        const bool keepStateId
    ) {
        if (keepStateId) {
            return stateId;
        }
    
        if (!bsr.IsValidState(stateId)) {
            return stateId;
        }

        const uint16_t placedTypeId = bsr.GetTypeIdFromState(stateId);
        const auto& placedType = bsr.GetType(placedTypeId);
        if (!detail::IsFenceTypeName(placedType.name)) {
            return stateId;
        }

        return detail::FinalizeFencePlacedState(bsr, chunkManager, stateId, position);
    }

    uint32_t FinalizePlacedState(
        const blockstate::BlockStateRegistry& bsr,
        const ChunkManager& chunkManager,
        uint32_t stateId,
        int x, int y, int z,
        PlacementContext context,
        const bool keepStateId
    ) {
        return FinalizePlacedState(bsr, chunkManager, stateId, WorldCoord(x, y, z), context, keepStateId);
    }

    uint32_t FinalizePlacedStateBasic(
        const blockstate::BlockStateRegistry& bsr,
        uint32_t stateId,
        const WorldCoord& position,
        PlacementContext context,
        const bool keepStateId
    ) {
        (void)bsr;
        (void)position;
        (void)context;
        (void)keepStateId;
        return stateId;
    }

    uint32_t FinalizePlacedStateBasic(
        const blockstate::BlockStateRegistry& bsr,
        uint32_t stateId,
        int x, int y, int z,
        PlacementContext context,
        const bool keepStateId
    ) {
        return FinalizePlacedStateBasic(bsr, stateId, WorldCoord(x, y, z), context, keepStateId);
    }
}
