#pragma once

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

#include <glm/vec3.hpp>

namespace chunkutil {
    inline bool IsValidBlockCoord(int x, int y, int z) {
        return x >= 0 && x < sizes::CHUNK_SIZE && 
            y >= 0 && y < sizes::CHUNK_SIZE && 
            z >= 0 && z < sizes::CHUNK_SIZE;
    }

    inline int GetBlockIndex(int x, int y, int z) {
        return x + y * sizes::CHUNK_SIZE + z * sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
    }

    bool IsOnChunkFaceBoundary(const glm::ivec3& localPos, FaceDir face);
    bool TryWrapCrossChunkLocal(int& x, int& y, int& z, FaceDir& outAcrossFace);
}