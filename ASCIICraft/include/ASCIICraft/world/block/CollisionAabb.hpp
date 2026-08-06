#pragma once

#include <vector>

#include <glm/vec3.hpp>

namespace blockstate {

/// Axis-aligned collision box in local block space [0,1] relative to the block origin.
struct CollisionAabb {
    glm::vec3 min{0.0f, 0.0f, 0.0f};
    glm::vec3 max{1.0f, 1.0f, 1.0f};

    bool operator==(const CollisionAabb& o) const {
        return min == o.min && max == o.max;
    }

    bool operator!=(const CollisionAabb& o) const {
        return !(*this == o);
    }
};

inline CollisionAabb MakeFullBlockCollisionAabb() {
    return CollisionAabb{glm::vec3(0.0f), glm::vec3(1.0f)};
}

inline bool AabbsOverlap(
    const glm::vec3& aMin,
    const glm::vec3& aMax,
    const glm::vec3& bMin,
    const glm::vec3& bMax
) {
    return aMin.x < bMax.x && aMax.x > bMin.x &&
           aMin.y < bMax.y && aMax.y > bMin.y &&
           aMin.z < bMax.z && aMax.z > bMin.z;
}

/// Translate a local-block collision box into world space at integer block position.
inline void CollisionAabbWorldBounds(
    const CollisionAabb& local,
    int blockX,
    int blockY,
    int blockZ,
    glm::vec3& outMin,
    glm::vec3& outMax
) {
    const glm::vec3 origin(static_cast<float>(blockX), static_cast<float>(blockY), static_cast<float>(blockZ));
    outMin = origin + local.min;
    outMax = origin + local.max;
}

} // namespace blockstate
