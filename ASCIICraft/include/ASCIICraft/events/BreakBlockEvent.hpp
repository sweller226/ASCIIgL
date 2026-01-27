#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Coords.hpp>

struct BreakBlockEvent {
    Block* block;
    WorldCoord position;
};
