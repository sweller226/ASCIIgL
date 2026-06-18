#include <ASCIICraft/world/block/placement/FencePlacement.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>
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
    uint32_t outStateId = stateId;

    for (FaceDir dir : kHorizontalFaceDirs) {
        const WorldCoord neighborPos = NeighborCoord(position, dir);
        const uint32_t neighborStateId = chunkManager.GetBlockState(neighborPos);
        const bool connected = ShouldFenceConnectToNeighbor(bsr, neighborStateId);
        outStateId = bsr.WithProperty(outStateId, FaceDirToString(dir), connected ? "true" : "false");
    }

    return outStateId;
}

} // namespace blockplacement::detail
