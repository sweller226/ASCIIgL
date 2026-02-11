#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <ASCIICraft/world/blockstate/BlockProperty.hpp>

namespace blockstate {

/// Registered block type definition.
/// Replaces the old BlockType enum. Each type is assigned an ID by the registry.
struct BlockType {
    uint16_t typeId = 0;
    std::string name;                        // e.g. "stone", "furnace"
    std::vector<BlockProperty> properties;

    uint32_t baseStateId = 0;                // first flattened state ID
    uint32_t stateCount = 1;                 // product of all property cardinalities
    uint32_t defaultStateId = 0;             // baseStateId + default property offset

    bool hasBlockEntity = false;             // true for chests, furnaces, signs, etc.
};

} // namespace blockstate
