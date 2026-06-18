#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

namespace chunkutil {

bool IsOnChunkFaceBoundary(const glm::ivec3& localPos, FaceDir face) {
    const glm::ivec3 offset = FaceDirNeighborOffset(face);
    if (offset.x > 0) return localPos.x == sizes::CHUNK_SIZE - 1;
    if (offset.x < 0) return localPos.x == 0;
    if (offset.y > 0) return localPos.y == sizes::CHUNK_SIZE - 1;
    if (offset.y < 0) return localPos.y == 0;
    if (offset.z > 0) return localPos.z == sizes::CHUNK_SIZE - 1;
    if (offset.z < 0) return localPos.z == 0;
    return false;
}

bool TryWrapCrossChunkLocal(int& x, int& y, int& z, FaceDir& outAcrossFace) {
    if (x >= 0 && x < sizes::CHUNK_SIZE &&
        y >= 0 && y < sizes::CHUNK_SIZE &&
        z >= 0 && z < sizes::CHUNK_SIZE) {
        return false;
    }

    if (x < 0) {
        outAcrossFace = FaceDir::West;
        x = sizes::CHUNK_SIZE - 1;
    } else if (x >= sizes::CHUNK_SIZE) {
        outAcrossFace = FaceDir::East;
        x = 0;
    } else if (y < 0) {
        outAcrossFace = FaceDir::Bottom;
        y = sizes::CHUNK_SIZE - 1;
    } else if (y >= sizes::CHUNK_SIZE) {
        outAcrossFace = FaceDir::Top;
        y = 0;
    } else if (z < 0) {
        outAcrossFace = FaceDir::North;
        z = sizes::CHUNK_SIZE - 1;
    } else {
        outAcrossFace = FaceDir::South;
        z = 0;
    }
    return true;
}

} // namespace chunkutil
