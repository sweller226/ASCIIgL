#pragma once

#include <cstdint>
#include <ASCIICraft/world/Coords.hpp>

namespace events {

struct PlaceBlockEvent {
    uint32_t stateId;    // blockstate to place
    WorldCoord position;
};
    
}