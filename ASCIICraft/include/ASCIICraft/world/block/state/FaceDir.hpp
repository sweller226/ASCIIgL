#pragma once

#include <cstdint>
#include <string>

#include <glm/vec2.hpp>

/// Block face indices — extracted from old Block.hpp for shared use.
enum class FaceDir : uint8_t {
    Top = 0,     // +Y
    Bottom = 1,  // -Y
    North = 2,   // Front (+Z)
    South = 3,   // Back (-Z)
    East = 4,    // Right (+X)
    West = 5     // Left (-X)
};

/// Convert FaceDir enum to blockstate property string (for "facing" property).
/// Only horizontal faces (North/South/East/West) are valid for facing.
/// Returns "north" as default for invalid faces.
inline const char* FaceDirToString(FaceDir face) {
    switch (face) {
        case FaceDir::North: return "north";
        case FaceDir::South: return "south";
        case FaceDir::East:  return "east";
        case FaceDir::West:  return "west";
        default:               return "north"; // Top/Bottom not valid for facing
    }
}

/// Convert blockstate property string to FaceDir enum.
/// Returns FaceDir::North as default for invalid strings.
inline FaceDir StringToFaceDir(const std::string& str) {
    if (str == "north") return FaceDir::North;
    if (str == "south") return FaceDir::South;
    if (str == "east")  return FaceDir::East;
    if (str == "west")  return FaceDir::West;
    return FaceDir::North; // Default fallback
}

/// Rotate top/bottom face UV based on block facing (North = default, no rotation).
/// Use for blocks with a "facing" property so the top/bottom texture aligns with the direction.
inline glm::vec2 RotateTopBottomUV(glm::vec2 uv, FaceDir facing) {
    float u = uv.x, v = uv.y;
    switch (facing) {
        case FaceDir::North: return uv;                    // 0°
        case FaceDir::South: return glm::vec2(1.0f - u, 1.0f - v); // 180°
        case FaceDir::East:  return glm::vec2(1.0f - v, u);        // 90° CW
        case FaceDir::West:  return glm::vec2(v, 1.0f - u);        // 270° CW
        default:             return uv;
    }
}
