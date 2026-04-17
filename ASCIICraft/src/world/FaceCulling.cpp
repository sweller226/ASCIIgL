#include <ASCIICraft/world/FaceCulling.hpp>

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/ChunkUtil.hpp>

static uint32_t GetBlockStateAt(
    int x, int y, int z,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks
) {
    if (chunkutil::IsValidBlockCoord(x, y, z)) {
        return chunkBlocks[chunkutil::GetBlockIndex(x, y, z)];
    }
    int neighborChunkDir = -1;
    int localX = x, localY = y, localZ = z;
    if (x < 0) {
        neighborChunkDir = 5;
        localX = sizes::CHUNK_SIZE - 1;
    } else if (x >= sizes::CHUNK_SIZE) {
        neighborChunkDir = 4;
        localX = 0;
    } else if (y < 0) {
        neighborChunkDir = 1;
        localY = sizes::CHUNK_SIZE - 1;
    } else if (y >= sizes::CHUNK_SIZE) {
        neighborChunkDir = 0;
        localY = 0;
    } else if (z < 0) {
        neighborChunkDir = 3;
        localZ = sizes::CHUNK_SIZE - 1;
    } else if (z >= sizes::CHUNK_SIZE) {
        neighborChunkDir = 2;
        localZ = 0;
    }
    if (neighborChunkDir >= 0 && neighborBlocks[neighborChunkDir]) {
        return neighborBlocks[neighborChunkDir][chunkutil::GetBlockIndex(localX, localY, localZ)];
    }
    return blockstate::BlockStateRegistry::AIR_STATE_ID;
}

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

        for (int face = 0; face < 6; ++face) {
            int neighborX = x, neighborY = y, neighborZ = z;
            switch (face) {
                case 0: neighborY++; break;
                case 1: neighborY--; break;
                case 2: neighborZ++; break;
                case 3: neighborZ--; break;
                case 4: neighborX++; break;
                case 5: neighborX--; break;
            }

            uint32_t neighborStateId = GetBlockStateAt(
                neighborX, neighborY, neighborZ,
                chunkBlocks, neighborBlocks
            );

            const auto& neighborState = bsr.GetState(neighborStateId);

            const bool baseNeighborOccludes =
                (neighborState.isRenderable && neighborState.renderMode == blockstate::RenderMode::Opaque) ||
                (state.cullSameType && neighborState.typeId == state.typeId);
            const bool neighborOccludes = baseNeighborOccludes && neighborState.isFullBlock;

            visibleFaces[face] = !neighborOccludes;
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

        for (int face = 0; face < 6; ++face) {
            int neighborX = x, neighborY = y, neighborZ = z;
            switch (face) {
                case 0: neighborY++; break;
                case 1: neighborY--; break;
                case 2: neighborZ++; break;
                case 3: neighborZ--; break;
                case 4: neighborX++; break;
                case 5: neighborX--; break;
            }

            const uint32_t neighborStateId = GetBlockStateAt(
                neighborX, neighborY, neighborZ,
                chunkBlocks, neighborBlocks
            );
            const auto& neighborState = bsr.GetState(neighborStateId);

            // Water should not render internal faces against adjacent water,
            // regardless of per-state shape differences (top/full variants).
            if (neighborState.typeId == state.typeId) {
                visibleFaces[face] = false;
                continue;
            }

            // Also cull against opaque full blocks.
            const bool neighborOccludes =
                neighborState.isRenderable &&
                neighborState.renderMode == blockstate::RenderMode::Opaque &&
                neighborState.isFullBlock;

            visibleFaces[face] = !neighborOccludes;
        }
    }
}