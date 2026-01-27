#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Coords.hpp>

struct PlaceBlockEvent {
    Block block;
    WorldCoord position;
};
