#pragma once

#include <cstdint>
#include <string>

#include <ASCIICraft/world/Coords.hpp>

class ChunkManager;

namespace blockstate {
class BlockStateRegistry;
}

namespace blockplacement::detail {

uint32_t FinalizeFencePlacedState(
    const blockstate::BlockStateRegistry& bsr,
    const ChunkManager& chunkManager,
    uint32_t stateId,
    const WorldCoord& position
);

bool IsFenceTypeName(const std::string& typeName);

} // namespace blockplacement::detail

