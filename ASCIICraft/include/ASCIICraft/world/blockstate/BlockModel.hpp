#pragma once

#include <vector>

namespace blockstate {
    struct BlockModel {
        // True if the model fully occupies the block volume (a 1x1x1 cube).
        // Used by meshing to decide whether it can occlude neighbors.
        bool isFullBlock = false;
        std::vector<std::byte> opaqueVertices;
        std::vector<int> opaqueIndices;
        std::vector<std::byte> transparentVertices;
        std::vector<int> transparentIndices;
    };
}

