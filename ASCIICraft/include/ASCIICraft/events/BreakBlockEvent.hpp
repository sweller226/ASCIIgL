#pragma once

#include <cstdint>
#include <ASCIICraft/world/Coords.hpp>

namespace events {

struct BreakBlockEvent {
    uint32_t stateId;    // blockstate of the broken block (for drops, etc.)
    WorldCoord position;
    /// False when the block was mined without a sufficient tool (wrong class or
    /// harvest level too low): the block still breaks but drops nothing.
    bool harvested = true;
};

}


