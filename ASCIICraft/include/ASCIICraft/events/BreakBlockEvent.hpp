#pragma once

#include <cstdint>
#include <ASCIICraft/world/Coords.hpp>

namespace events {

struct BreakBlockEvent {
    uint32_t stateId;    // blockstate of the broken block (for drops, etc.)
    WorldCoord position;
};

}


