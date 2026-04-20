#pragma once

#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace faceculling {

void ComputeVisibleFacesFullBlock(
    int x, int y, int z,
    const blockstate::BlockState& state,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    const blockstate::BlockStateRegistry& bsr,
    std::vector<bool>& visibleFaces
);

void ComputeVisibleFacesWater(
    int x, int y, int z,
    const blockstate::BlockState& state,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    const blockstate::BlockStateRegistry& bsr,
    std::vector<bool>& visibleFaces
);

}