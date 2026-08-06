#include <ASCIICraft/world/query/VoxelOverlap.hpp>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/CollisionAabb.hpp>
#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace worldquery {

namespace {

bool EntityOverlapsStateCollision(
    const blockstate::BlockStateRegistry *bsr,
    uint32_t stateId,
    int blockX,
    int blockY,
    int blockZ,
    const glm::vec3 &entityMin,
    const glm::vec3 &entityMax
) {
    if (!blockquery::IsSolidForPhysics(bsr, stateId)) {
        return false;
    }

    const blockstate::BlockState *state = nullptr;
    if (blockquery::TryGetState(bsr, stateId, state) && !state->collisionBoxes.empty()) {
        for (const blockstate::CollisionAabb &local : state->collisionBoxes) {
            glm::vec3 boxMin;
            glm::vec3 boxMax;
            blockstate::CollisionAabbWorldBounds(local, blockX, blockY, blockZ, boxMin, boxMax);
            if (blockstate::AabbsOverlap(entityMin, entityMax, boxMin, boxMax)) {
                return true;
            }
        }
        return false;
    }

    // Fallback: full 1³ cell (legacy / missing boxes on full blocks).
    const glm::vec3 cellMin(static_cast<float>(blockX), static_cast<float>(blockY), static_cast<float>(blockZ));
    const glm::vec3 cellMax = cellMin + glm::vec3(1.0f);
    return blockstate::AabbsOverlap(entityMin, entityMax, cellMin, cellMax);
}

} // namespace

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

    const glm::vec3 entityMin = center - halfExtents;
    const glm::vec3 entityMax = center + halfExtents;

    const glm::ivec3 imin = glm::floor(entityMin);
    const glm::ivec3 imax = glm::floor(entityMax);

    for (int x = imin.x; x <= imax.x; ++x) {
        for (int y = imin.y; y <= imax.y; ++y) {
            for (int z = imin.z; z <= imax.z; ++z) {
                const uint32_t stateId = world->GetBlockState(x, y, z);
                if (EntityOverlapsStateCollision(bsr, stateId, x, y, z, entityMin, entityMax)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace worldquery
