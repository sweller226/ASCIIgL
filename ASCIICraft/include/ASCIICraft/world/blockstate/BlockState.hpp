#pragma once

#include <cstdint>
#include <vector>

#include <ASCIICraft/world/blockstate/BlockFace.hpp>

namespace blockstate {

/// One flattened blockstate — unique (type + property values) combination.
/// Derived data is pre-computed at registration time for hot-path access.
struct BlockState {
    uint32_t stateId = 0;
    uint16_t typeId = 0;
    std::vector<uint8_t> propertyValues;     // index into each property's allowedValues

    // Pre-computed derived data
    bool isSolid = true;
    bool isTransparent = false;
    int faceTextureLayers[BLOCK_FACE_COUNT] = {0, 0, 0, 0, 0, 0};
    uint8_t lightEmission = 0;
    uint8_t lightFilter = 15;                // how much light this block absorbs
};

} // namespace blockstate
