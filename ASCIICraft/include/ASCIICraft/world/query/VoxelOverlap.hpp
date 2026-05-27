#pragma once

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include <ASCIICraft/world/query/BlockQueries.hpp>

class World;

namespace blockstate {
class BlockStateRegistry;
} // namespace blockstate

namespace worldquery {

struct VoxelOverlapHit {
    glm::ivec3 blockPos{};
    uint32_t stateId = 0;
    uint16_t typeId = 0;
};

/// Distance below the collider bottom used by floor sampling (matches physics ground probe).
inline constexpr float FLOOR_PROBE_DISTANCE = 0.05f;

/// Block under the bottom of \p center / \p halfExtents (feet probe, not full AABB overlap).
VoxelOverlapHit SampleFloorBlock(
    const World& world,
    const glm::vec3& center,
    const glm::vec3& halfExtents,
    float probeDistance = FLOOR_PROBE_DISTANCE
);

/// First overlapping voxel in scan order, or empty if none match \p filter.
/// Null \p filter matches any non-air block.
std::optional<VoxelOverlapHit> QueryVoxelOverlap(
    const World &world,
    const glm::vec3 &center,
    const glm::vec3 &halfExtents,
    blockquery::BlockFilter filter = nullptr
);

/// Enriches \p hit.typeId when \p bsr is available.
void EnrichHitWithTypeId(const blockstate::BlockStateRegistry *bsr, VoxelOverlapHit &hit);

/// Physics AABB overlap using full-block collision rules.
bool OverlapsSolidForPhysics(
    const World *world,
    const blockstate::BlockStateRegistry *bsr,
    const glm::vec3 &center,
    const glm::vec3 &halfExtents,
    bool colliderDisabled
);

} // namespace worldquery
