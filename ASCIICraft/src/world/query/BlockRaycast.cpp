#include <ASCIICraft/world/query/BlockRaycast.hpp>

#include <ASCIICraft/world/block/CollisionAabb.hpp>
#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/query/BlockQueries.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace worldquery {

namespace {

constexpr float kRayEps = 1e-8f;

FaceDir FaceFromAxisMin(int axis) {
    switch (axis) {
        case 0: return FaceDir::West;
        case 1: return FaceDir::Bottom;
        default: return FaceDir::North;
    }
}

FaceDir FaceFromAxisMax(int axis) {
    switch (axis) {
        case 0: return FaceDir::East;
        case 1: return FaceDir::Top;
        default: return FaceDir::South;
    }
}

const std::vector<blockstate::CollisionAabb>& SelectionBoxesForState(
    const blockstate::BlockStateRegistry* bsr,
    uint32_t stateId
) {
    static const std::vector<blockstate::CollisionAabb> kFullCell = {
        blockstate::MakeFullBlockCollisionAabb()
    };

    const blockstate::BlockState* state = nullptr;
    if (blockquery::TryGetState(bsr, stateId, state) && !state->collisionBoxes.empty()) {
        return state->collisionBoxes;
    }
    return kFullCell;
}

std::optional<BlockRayHit> TestCellSelection(
    const blockstate::BlockStateRegistry* bsr,
    uint32_t stateId,
    int bx,
    int by,
    int bz,
    const glm::vec3& origin,
    const glm::vec3& dir,
    float reach
) {
    if (blockquery::IsAir(stateId)) {
        return std::nullopt;
    }

    const auto& boxes = SelectionBoxesForState(bsr, stateId);

    std::optional<BlockRayHit> best;
    for (const blockstate::CollisionAabb& local : boxes) {
        glm::vec3 boxMin;
        glm::vec3 boxMax;
        blockstate::CollisionAabbWorldBounds(local, bx, by, bz, boxMin, boxMax);

        const auto hit = RayAabb(origin, dir, boxMin, boxMax, reach);
        if (!hit.has_value()) {
            continue;
        }

        const float t = std::max(hit->tEnter, 0.0f);
        if (t > reach) {
            continue;
        }
        if (!best.has_value() || t < best->t) {
            best = BlockRayHit{
                stateId,
                WorldCoord(bx, by, bz),
                hit->face,
                t
            };
        }
    }
    return best;
}

} // namespace

std::optional<RayAabbHit> RayAabb(
    const glm::vec3& origin,
    const glm::vec3& dir,
    const glm::vec3& boxMin,
    const glm::vec3& boxMax,
    float tMax
) {
    float tEnter = -std::numeric_limits<float>::infinity();
    float tExit = std::numeric_limits<float>::infinity();
    FaceDir enterFace = FaceDir::North;

    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float bmin[3] = {boxMin.x, boxMin.y, boxMin.z};
    const float bmax[3] = {boxMax.x, boxMax.y, boxMax.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(d[axis]) < kRayEps) {
            if (o[axis] < bmin[axis] || o[axis] > bmax[axis]) {
                return std::nullopt;
            }
            continue;
        }

        float t1 = (bmin[axis] - o[axis]) / d[axis];
        float t2 = (bmax[axis] - o[axis]) / d[axis];
        FaceDir f1 = FaceFromAxisMin(axis);
        FaceDir f2 = FaceFromAxisMax(axis);
        if (t1 > t2) {
            std::swap(t1, t2);
            std::swap(f1, f2);
        }

        if (t1 > tEnter) {
            tEnter = t1;
            enterFace = f1;
        }
        tExit = std::min(tExit, t2);

        if (tEnter > tExit) {
            return std::nullopt;
        }
    }

    // Segment [0, tMax] must overlap [tEnter, tExit].
    if (tExit < 0.0f || tEnter > tMax) {
        return std::nullopt;
    }

    return RayAabbHit{tEnter, enterFace};
}

std::optional<BlockRayHit> RaycastBlocks(
    const BlockStateGetter& getState,
    const blockstate::BlockStateRegistry* bsr,
    const glm::vec3& origin,
    const glm::vec3& dirIn,
    float reach
) {
    if (reach <= 0.0f || !getState) {
        return std::nullopt;
    }

    const float dirLen = glm::length(dirIn);
    if (dirLen < kRayEps) {
        return std::nullopt;
    }
    const glm::vec3 dir = dirIn / dirLen;

    // Amanatides & Woo grid traversal; test selection AABBs in each visited cell.
    glm::ivec3 cell(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z))
    );

    glm::ivec3 step(0);
    glm::vec3 tMax(std::numeric_limits<float>::infinity());
    glm::vec3 tDelta(std::numeric_limits<float>::infinity());

    auto initAxis = [&](int axis, float originA, float dirA, int& cellA) {
        if (dirA > kRayEps) {
            step[axis] = 1;
            const float nextBoundary = static_cast<float>(cellA + 1);
            tMax[axis] = (nextBoundary - originA) / dirA;
            tDelta[axis] = 1.0f / dirA;
        } else if (dirA < -kRayEps) {
            step[axis] = -1;
            const float nextBoundary = static_cast<float>(cellA);
            tMax[axis] = (nextBoundary - originA) / dirA;
            tDelta[axis] = -1.0f / dirA;
        } else {
            step[axis] = 0;
            tMax[axis] = std::numeric_limits<float>::infinity();
            tDelta[axis] = std::numeric_limits<float>::infinity();
        }
    };

    initAxis(0, origin.x, dir.x, cell.x);
    initAxis(1, origin.y, dir.y, cell.y);
    initAxis(2, origin.z, dir.z, cell.z);

    // Bound iterations: at most one step per unit axis along reach, plus slack.
    const int maxSteps = static_cast<int>(std::ceil(reach * 3.0f)) + 8;

    for (int i = 0; i < maxSteps; ++i) {
        const uint32_t stateId = getState(cell.x, cell.y, cell.z);
        if (auto hit = TestCellSelection(bsr, stateId, cell.x, cell.y, cell.z, origin, dir, reach)) {
            return hit;
        }

        // Advance to next voxel; stop once the boundary crossed is beyond reach.
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                if (tMax.x > reach) {
                    break;
                }
                cell.x += step.x;
                tMax.x += tDelta.x;
            } else {
                if (tMax.z > reach) {
                    break;
                }
                cell.z += step.z;
                tMax.z += tDelta.z;
            }
        } else {
            if (tMax.y < tMax.z) {
                if (tMax.y > reach) {
                    break;
                }
                cell.y += step.y;
                tMax.y += tDelta.y;
            } else {
                if (tMax.z > reach) {
                    break;
                }
                cell.z += step.z;
                tMax.z += tDelta.z;
            }
        }

        if (step.x == 0 && step.y == 0 && step.z == 0) {
            break;
        }
    }

    return std::nullopt;
}

} // namespace worldquery
