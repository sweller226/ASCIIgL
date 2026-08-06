#include <ASCIICraft/world/block/models/BlockModelCollision.hpp>

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace blockmodels {

namespace {

constexpr float kModelCoordToBlock = 1.0f / 16.0f;
/// Skip elements thinner than this in any model-space axis (plants/cross planes).
constexpr float kDegenerateExtentEps = 1e-3f;

glm::vec3 RotateAxis(const glm::vec3& p, const glm::vec3& origin, char axis, float angleDeg) {
    const float r = angleDeg * (3.1415926535f / 180.0f);
    const float c = std::cos(r);
    const float s = std::sin(r);
    const glm::vec3 t = p - origin;
    glm::vec3 out = t;
    switch (axis) {
        case 'x': out = glm::vec3(t.x, t.y * c - t.z * s, t.y * s + t.z * c); break;
        case 'y': out = glm::vec3(t.x * c - t.z * s, t.y, t.x * s + t.z * c); break;
        case 'z': out = glm::vec3(t.x * c - t.y * s, t.x * s + t.y * c, t.z); break;
        default: break;
    }
    return out + origin;
}

glm::vec3 ApplyElementRotationModelSpace(const glm::vec3& p, const ResolvedBlockModelElement& elem) {
    if (!elem.rotation.has_value()) {
        return p;
    }
    const auto& r = elem.rotation.value();
    const glm::vec3 origin(r.origin[0], r.origin[1], r.origin[2]);
    glm::vec3 out = RotateAxis(p, origin, r.axis, r.angle);
    if (r.rescale) {
        glm::vec3 o = origin;
        glm::vec3 d = out - o;
        float k = 1.0f;
        if (r.axis == 'x' || r.axis == 'y' || r.axis == 'z') {
            k = 1.0f / std::max(0.0001f, std::abs(std::cos(r.angle * (3.1415926535f / 180.0f))));
        }
        if (r.axis == 'x') d = glm::vec3(d.x, d.y * k, d.z * k);
        if (r.axis == 'y') d = glm::vec3(d.x * k, d.y, d.z * k);
        if (r.axis == 'z') d = glm::vec3(d.x * k, d.y * k, d.z);
        out = o + d;
    }
    return out;
}

glm::vec3 ApplyVariantRotationsBlockSpace(const glm::vec3& p, int variantX, int variantY) {
    const glm::vec3 c(0.5f, 0.5f, 0.5f);
    glm::vec3 out = p;
    if (variantX != 0) {
        out = RotateAxis(out, c, 'x', static_cast<float>(variantX));
    }
    if (variantY != 0) {
        out = RotateAxis(out, c, 'y', static_cast<float>(variantY));
    }
    return out;
}

bool IsDegenerateElement(const ResolvedBlockModelElement& elem) {
    const float dx = std::abs(elem.to[0] - elem.from[0]);
    const float dy = std::abs(elem.to[1] - elem.from[1]);
    const float dz = std::abs(elem.to[2] - elem.from[2]);
    return dx < kDegenerateExtentEps || dy < kDegenerateExtentEps || dz < kDegenerateExtentEps;
}

blockstate::CollisionAabb ElementToCollisionAabb(
    const ResolvedBlockModelElement& elem,
    int variantX,
    int variantY
) {
    const float x1 = std::min(elem.from[0], elem.to[0]);
    const float y1 = std::min(elem.from[1], elem.to[1]);
    const float z1 = std::min(elem.from[2], elem.to[2]);
    const float x2 = std::max(elem.from[0], elem.to[0]);
    const float y2 = std::max(elem.from[1], elem.to[1]);
    const float z2 = std::max(elem.from[2], elem.to[2]);

    const glm::vec3 cornersModel[8] = {
        {x1, y1, z1}, {x1, y1, z2}, {x1, y2, z1}, {x1, y2, z2},
        {x2, y1, z1}, {x2, y1, z2}, {x2, y2, z1}, {x2, y2, z2},
    };

    glm::vec3 mn(1e30f);
    glm::vec3 mx(-1e30f);
    for (const glm::vec3& corner : cornersModel) {
        // Same transform stack as BakeResolvedModel:
        // 1) element rotation in model space
        // 2) normalize 0..16 to 0..1 block space
        // 3) variant x/y rotation in block space
        glm::vec3 p = ApplyElementRotationModelSpace(corner, elem);
        p *= kModelCoordToBlock;
        p = ApplyVariantRotationsBlockSpace(p, variantX, variantY);
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }

    return blockstate::CollisionAabb{mn, mx};
}

} // namespace

std::vector<blockstate::CollisionAabb> BuildCollisionBoxes(
    const ResolvedBlockModelDefinition& resolved,
    int variantX,
    int variantY
) {
    // Full-cube models keep a single unit box (fast path / exact match to legacy physics).
    if (resolved.isFullBlock) {
        return {blockstate::MakeFullBlockCollisionAabb()};
    }

    std::vector<blockstate::CollisionAabb> boxes;
    boxes.reserve(resolved.elements.size());

    for (const auto& elem : resolved.elements) {
        if (IsDegenerateElement(elem)) {
            continue;
        }
        boxes.push_back(ElementToCollisionAabb(elem, variantX, variantY));
    }

    return boxes;
}

} // namespace blockmodels
