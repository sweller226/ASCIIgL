#pragma once

#include <glm/vec3.hpp>

namespace rendering::items {

/// Minecraft item/block model JSON uses 0..16 units per block face.
inline constexpr float kItemModelUnitsPerBlock = 16.0f;

/// Flat item plate thickness in model space (Minecraft display layer uses 7.5..8.5 = 1 unit).
inline constexpr float kItemPlateCenterZ = 8.0f;
inline constexpr float kItemPlateThickness = 1.5f;
inline constexpr float kItemPlateMinZ = kItemPlateCenterZ - kItemPlateThickness * 0.5f;
inline constexpr float kItemPlateMaxZ = kItemPlateCenterZ + kItemPlateThickness * 0.5f;

/// Convert 0..16 model units to block space centered on the origin (-0.5..0.5).
inline glm::vec3 ModelUnitsToBlockCentered(float x, float y, float z) {
    const float toBlock = 1.0f / kItemModelUnitsPerBlock;
    return glm::vec3(
        x * toBlock - 0.5f,
        y * toBlock - 0.5f,
        z * toBlock - 0.5f
    );
}

} // namespace rendering::items
