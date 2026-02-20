#pragma once

#include <cstdint>
#include <string>

#include <glm/vec2.hpp>

/// Block face indices — extracted from old Block.hpp for shared use.
enum class BlockFace : uint8_t {
    Top = 0,
    Bottom = 1,
    North = 2,   // Front (+Z)
    South = 3,   // Back (-Z)
    East = 4,    // Right (+X)
    West = 5     // Left (-X)
};

static constexpr int BLOCK_FACE_COUNT = 6;

/// Convert BlockFace enum to blockstate property string (for "facing" property).
/// Only horizontal faces (North/South/East/West) are valid for facing.
/// Returns "north" as default for invalid faces.
inline const char* BlockFaceToString(BlockFace face) {
    switch (face) {
        case BlockFace::North: return "north";
        case BlockFace::South: return "south";
        case BlockFace::East:  return "east";
        case BlockFace::West:  return "west";
        default:               return "north"; // Top/Bottom not valid for facing
    }
}

/// Convert blockstate property string to BlockFace enum.
/// Returns BlockFace::North as default for invalid strings.
inline BlockFace StringToBlockFace(const std::string& str) {
    if (str == "north") return BlockFace::North;
    if (str == "south") return BlockFace::South;
    if (str == "east")  return BlockFace::East;
    if (str == "west")  return BlockFace::West;
    return BlockFace::North; // Default fallback
}

/// Rotate top/bottom face UV based on block facing (North = default, no rotation).
/// Use for blocks with a "facing" property so the top/bottom texture aligns with the direction.
inline glm::vec2 RotateTopBottomUV(glm::vec2 uv, BlockFace facing) {
    float u = uv.x, v = uv.y;
    switch (facing) {
        case BlockFace::North: return uv;                    // 0°
        case BlockFace::South: return glm::vec2(1.0f - u, 1.0f - v); // 180°
        case BlockFace::East:  return glm::vec2(1.0f - v, u);        // 90° CW
        case BlockFace::West:  return glm::vec2(v, 1.0f - u);        // 270° CW
        default:               return uv;
    }
}
