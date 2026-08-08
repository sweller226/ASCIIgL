#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <glm/vec3.hpp>

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

namespace blockstate {
class BlockStateRegistry;
} // namespace blockstate

namespace worldquery {

struct BlockRayHit {
    uint32_t stateId = 0;
    WorldCoord blockPos{};
    FaceDir face = FaceDir::North;
    float t = 0.0f;
};

struct RayAabbHit {
    float tEnter = 0.0f;
    FaceDir face = FaceDir::North;
};

/// Ray–AABB intersection (slab method). Returns entry distance and entry face when the segment
/// [0, tMax] overlaps the box. Origin inside the box yields tEnter <= 0 (caller may clamp).
std::optional<RayAabbHit> RayAabb(
    const glm::vec3& origin,
    const glm::vec3& dir,
    const glm::vec3& boxMin,
    const glm::vec3& boxMax,
    float tMax
);

using BlockStateGetter = std::function<uint32_t(int x, int y, int z)>;

/// Nearest block selection hit along a normalized ray within \p reach.
/// Selection boxes: BlockState::collisionBoxes when non-empty; otherwise a full 1³ cell
/// for any non-air block (plants remain targetable while physics-non-solid).
std::optional<BlockRayHit> RaycastBlocks(
    const BlockStateGetter& getState,
    const blockstate::BlockStateRegistry* bsr,
    const glm::vec3& origin,
    const glm::vec3& dir,
    float reach
);

} // namespace worldquery
