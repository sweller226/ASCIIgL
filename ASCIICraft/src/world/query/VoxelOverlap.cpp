#include <ASCIICraft/world/query/VoxelOverlap.hpp>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace worldquery {

void EnrichHitWithTypeId(const blockstate::BlockStateRegistry *bsr, VoxelOverlapHit &hit) {
    hit.typeId = bsr ? bsr->GetTypeIdFromStateOr(hit.stateId) : 0;
}

VoxelOverlapHit SampleFloorBlock(
    const World& world,
    const glm::vec3& center,
    const glm::vec3& halfExtents,
    float probeDistance
) {
    const glm::vec3 probePos(
        center.x,
        center.y - halfExtents.y - probeDistance,
        center.z
    );
    const glm::ivec3 blockPos = glm::floor(probePos);
    const uint32_t stateId = world.GetBlockState(blockPos.x, blockPos.y, blockPos.z);
    return VoxelOverlapHit{blockPos, stateId, 0};
}

std::optional<VoxelOverlapHit> QueryVoxelOverlap(
    const World &world,
    const glm::vec3 &center,
    const glm::vec3 &halfExtents,
    blockquery::BlockFilter filter
) {
    const glm::vec3 min = center - halfExtents;
    const glm::vec3 max = center + halfExtents;

    const glm::ivec3 imin = glm::floor(min);
    const glm::ivec3 imax = glm::floor(max);

    for (int x = imin.x; x <= imax.x; ++x) {
        for (int y = imin.y; y <= imax.y; ++y) {
            for (int z = imin.z; z <= imax.z; ++z) {
                const uint32_t stateId = world.GetBlockState(x, y, z);
                const bool matches = filter ? filter(stateId) : blockquery::IsNonAir(stateId);
                if (matches) {
                    return VoxelOverlapHit{{x, y, z}, stateId, 0};
                }
            }
        }
    }
    return std::nullopt;
}

bool OverlapsSolidForPhysics(
    const World *world,
    const blockstate::BlockStateRegistry *bsr,
    const glm::vec3 &center,
    const glm::vec3 &halfExtents,
    bool colliderDisabled
) {
    if (colliderDisabled || !world) {
        return false;
    }
    return QueryVoxelOverlap(
        *world,
        center,
        halfExtents,
        blockquery::SolidForPhysicsFilter(bsr)
    ).has_value();
}

} // namespace worldquery
