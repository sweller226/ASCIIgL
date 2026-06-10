#include <ASCIICraft/world/block/placement/FencePlacement.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>

namespace blockplacement::detail {

bool IsFenceTypeName(const std::string& typeName) {
    return typeName == "minecraft:fence" || typeName == "minecraft:oak_fence";
}

namespace {
bool ShouldFenceConnectToNeighbor(
    const blockstate::BlockStateRegistry& bsr,
    const uint32_t neighborStateId
) {
    if (!bsr.IsValidState(neighborStateId)) return false;

    const auto& neighborState = bsr.GetState(neighborStateId);
    const uint16_t neighborTypeId = bsr.GetTypeIdFromState(neighborStateId);
    const auto& neighborType = bsr.GetType(neighborTypeId);

    if (IsFenceTypeName(neighborType.name)) {
        return true;
    }

    return neighborStateId != blockstate::BlockStateRegistry::AIR_STATE_ID && neighborState.isFullBlock;
}
} // namespace

uint32_t FinalizeFencePlacedState(
    const blockstate::BlockStateRegistry& bsr,
    const ChunkManager& chunkManager,
    uint32_t stateId,
    const WorldCoord& position
) {
    const WorldCoord eastPos(position.x + 1, position.y, position.z);
    const WorldCoord northPos(position.x, position.y, position.z - 1);
    const WorldCoord southPos(position.x, position.y, position.z + 1);
    const WorldCoord westPos(position.x - 1, position.y, position.z);

    const auto isFenceNeighbor = [&](const WorldCoord& p) {
        const uint32_t neighborStateId = chunkManager.GetBlockState(p);
        return ShouldFenceConnectToNeighbor(bsr, neighborStateId);
    };

    uint32_t outStateId = stateId;
    outStateId = bsr.WithProperty(outStateId, "east", isFenceNeighbor(eastPos) ? "true" : "false");
    outStateId = bsr.WithProperty(outStateId, "north", isFenceNeighbor(northPos) ? "true" : "false");
    outStateId = bsr.WithProperty(outStateId, "south", isFenceNeighbor(southPos) ? "true" : "false");
    outStateId = bsr.WithProperty(outStateId, "west", isFenceNeighbor(westPos) ? "true" : "false");
    return outStateId;
}

} // namespace blockplacement::detail

