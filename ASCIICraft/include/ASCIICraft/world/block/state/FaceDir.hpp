#pragma once

#include <cstdint>
#include <cmath>
#include <string>

#include <ASCIICraft/world/Coords.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

/// World-facing block face index. Values match FaceCulling / chunk mesh face order (0–5).
/// Horizontal directions: +X east, −X west, −Z north, +Z south, +Y top, −Y bottom.
enum class FaceDir : uint8_t {
    Top = 0,     // +Y
    Bottom = 1,  // -Y
    North = 2,   // -Z
    South = 3,   // +Z
    East = 4,    // +X
    West = 5     // -X
};

inline constexpr int kFaceCount = 6;

inline constexpr FaceDir kAllFaceDirs[] = {
    FaceDir::Top,
    FaceDir::Bottom,
    FaceDir::North,
    FaceDir::South,
    FaceDir::East,
    FaceDir::West,
};

inline constexpr FaceDir kHorizontalFaceDirs[] = {
    FaceDir::West,
    FaceDir::North,
    FaceDir::South,
    FaceDir::East,
};

inline FaceDir FaceDirFromIndex(int index) {
    return static_cast<FaceDir>(index);
}

inline int FaceDirToIndex(FaceDir face) {
    return static_cast<int>(face);
}

/// Block offset across a face (neighbor one step in that direction).
inline glm::ivec3 FaceDirNeighborOffset(FaceDir face) {
    switch (face) {
        case FaceDir::Top:    return {0, 1, 0};
        case FaceDir::Bottom: return {0, -1, 0};
        case FaceDir::North:  return {0, 0, -1};
        case FaceDir::South:  return {0, 0, 1};
        case FaceDir::East:   return {1, 0, 0};
        case FaceDir::West:  return {-1, 0, 0};
    }
    return {0, 0, 0};
}

inline glm::vec3 FaceDirOutwardNormal(FaceDir face) {
    const glm::ivec3 offset = FaceDirNeighborOffset(face);
    return glm::vec3(static_cast<float>(offset.x), static_cast<float>(offset.y), static_cast<float>(offset.z));
}

/// Map a (mostly axis-aligned) outward normal to the matching face index.
inline FaceDir FaceDirFromOutwardNormal(const glm::vec3& normal) {
    const glm::vec3 axis = glm::normalize(normal);
    const float ax = std::abs(axis.x);
    const float ay = std::abs(axis.y);
    const float az = std::abs(axis.z);
    if (ax > ay && ax > az) {
        return axis.x > 0.0f ? FaceDir::East : FaceDir::West;
    }
    if (ay > az) {
        return axis.y > 0.0f ? FaceDir::Top : FaceDir::Bottom;
    }
    return axis.z > 0.0f ? FaceDir::South : FaceDir::North;
}

inline WorldCoord NeighborCoord(const WorldCoord& position, FaceDir face) {
    const glm::ivec3 offset = FaceDirNeighborOffset(face);
    return WorldCoord(position.x + offset.x, position.y + offset.y, position.z + offset.z);
}

inline ChunkCoord NeighborChunkCoord(const ChunkCoord& chunk, FaceDir face) {
    const glm::ivec3 offset = FaceDirNeighborOffset(face);
    return chunk + ChunkCoord(offset.x, offset.y, offset.z);
}

/// Horizontal facing from a world-space direction (e.g. camera front).
inline FaceDir DominantHorizontalFaceDir(const glm::vec3& direction) {
    if (std::abs(direction.x) > std::abs(direction.z)) {
        return direction.x > 0.0f ? FaceDir::East : FaceDir::West;
    }
    return direction.z > 0.0f ? FaceDir::South : FaceDir::North;
}

/// Blockstate property name for horizontal faces ("facing", fence north/south/east/west, etc.).
inline const char* FaceDirToString(FaceDir face) {
    switch (face) {
        case FaceDir::North: return "north";
        case FaceDir::South: return "south";
        case FaceDir::East:  return "east";
        case FaceDir::West:  return "west";
        default:             return "north";
    }
}

inline const char* FaceDirCardinalLabel(FaceDir face) {
    switch (face) {
        case FaceDir::North: return "North";
        case FaceDir::South: return "South";
        case FaceDir::East:  return "East";
        case FaceDir::West:  return "West";
        default:             return "North";
    }
}

inline FaceDir StringToFaceDir(const std::string& str) {
    if (str == "north") return FaceDir::North;
    if (str == "south") return FaceDir::South;
    if (str == "east")  return FaceDir::East;
    if (str == "west")  return FaceDir::West;
    return FaceDir::North;
}

/// Rotate top/bottom face UV for blocks with a horizontal "facing" property (North = default).
inline glm::vec2 RotateTopBottomUV(glm::vec2 uv, FaceDir facing) {
    float u = uv.x, v = uv.y;
    switch (facing) {
        case FaceDir::North: return uv;
        case FaceDir::South: return glm::vec2(1.0f - u, 1.0f - v);
        case FaceDir::East:  return glm::vec2(1.0f - v, u);
        case FaceDir::West:  return glm::vec2(v, 1.0f - u);
        default:             return uv;
    }
}
