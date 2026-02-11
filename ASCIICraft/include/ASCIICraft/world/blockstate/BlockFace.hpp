#pragma once

#include <cstdint>

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
