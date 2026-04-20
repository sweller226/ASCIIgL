#pragma once

#include <ASCIICraft/world/Sizes.hpp>

namespace chunkutil {
    inline bool IsValidBlockCoord(int x, int y, int z) {
        return x >= 0 && x < sizes::CHUNK_SIZE && 
            y >= 0 && y < sizes::CHUNK_SIZE && 
            z >= 0 && z < sizes::CHUNK_SIZE;
    }

    // Helper methods
    inline int GetBlockIndex(int x, int y, int z) {
        return x + y * sizes::CHUNK_SIZE + z * sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
    }
}