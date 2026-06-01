#include <ASCIICraft/world/block/FaceCulling.hpp>

#include <array>

#include <ASCIICraft/world/block/state/FaceDir.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

namespace {

glm::ivec3 NeighborLocalBlockPos(int x, int y, int z, FaceDir face) {
    const glm::ivec3 offset = FaceDirNeighborOffset(face);
    return {x + offset.x, y + offset.y, z + offset.z};
}

uint32_t GetBlockStateAt(
    int x, int y, int z,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks
) {
    if (chunkutil::IsValidBlockCoord(x, y, z)) {
        return chunkBlocks[chunkutil::GetBlockIndex(x, y, z)];
    }

    FaceDir acrossFace = FaceDir::Top;
    if (!chunkutil::TryWrapCrossChunkLocal(x, y, z, acrossFace)) {
        return blockstate::BlockStateRegistry::AIR_STATE_ID;
    }

    const int neighborFaceIndex = FaceDirToIndex(acrossFace);
    if (neighborBlocks[neighborFaceIndex]) {
        return neighborBlocks[neighborFaceIndex][chunkutil::GetBlockIndex(x, y, z)];
    }
    return blockstate::BlockStateRegistry::AIR_STATE_ID;
}

} // namespace

namespace faceculling {
    void ComputeVisibleFacesFullBlock(
        int x, int y, int z,
        const blockstate::BlockState& state,
        const uint32_t* chunkBlocks,
        const std::array<const uint32_t*, 6>& neighborBlocks,
        const blockstate::BlockStateRegistry& bsr,
        std::vector<bool>& visibleFaces
    ) {
        visibleFaces.resize(6, false);

        for (int faceIndex = 0; faceIndex < kFaceCount; ++faceIndex) {
            const FaceDir face = FaceDirFromIndex(faceIndex);
            const glm::ivec3 neighborPos = NeighborLocalBlockPos(x, y, z, face);

            uint32_t neighborStateId = GetBlockStateAt(
                neighborPos.x, neighborPos.y, neighborPos.z,
                chunkBlocks, neighborBlocks
            );

            const auto& neighborState = bsr.GetState(neighborStateId);

            const bool baseNeighborOccludes =
                (neighborState.isRenderable && neighborState.renderMode == blockstate::RenderMode::Opaque) ||
                (state.cullSameType && neighborState.typeId == state.typeId);
            const bool neighborOccludes = baseNeighborOccludes && neighborState.isFullBlock;

            visibleFaces[faceIndex] = !neighborOccludes;
        }
    }

    void ComputeVisibleFacesWater(
        int x, int y, int z,
        const blockstate::BlockState& state,
        const uint32_t* chunkBlocks,
        const std::array<const uint32_t*, 6>& neighborBlocks,
        const blockstate::BlockStateRegistry& bsr,
        std::vector<bool>& visibleFaces
    ) {
        visibleFaces.resize(6, false);

        for (int faceIndex = 0; faceIndex < kFaceCount; ++faceIndex) {
            const FaceDir face = FaceDirFromIndex(faceIndex);
            const glm::ivec3 neighborPos = NeighborLocalBlockPos(x, y, z, face);

            const uint32_t neighborStateId = GetBlockStateAt(
                neighborPos.x, neighborPos.y, neighborPos.z,
                chunkBlocks, neighborBlocks
            );
            const auto& neighborState = bsr.GetState(neighborStateId);

            // Water should not render internal faces against adjacent water,
            // regardless of per-state shape differences (top/full variants).
            if (neighborState.typeId == state.typeId) {
                visibleFaces[faceIndex] = false;
                continue;
            }

            // Also cull against opaque full blocks.
            const bool neighborOccludes =
                neighborState.isRenderable &&
                neighborState.renderMode == blockstate::RenderMode::Opaque &&
                neighborState.isFullBlock;

            visibleFaces[faceIndex] = !neighborOccludes;
        }
    }
}
