#pragma once

#include <cstdint>
#include <ASCIICraft/world/Coords.hpp>

struct PlaceBlockEvent {
    uint32_t stateId;    // blockstate to place
    WorldCoord position;
};
