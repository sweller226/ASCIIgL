#pragma once

#include <vector>

#include <ASCIICraft/world/block/CollisionAabb.hpp>
#include <ASCIICraft/world/block/models/ResolvedBlockModel.hpp>

namespace blockmodels {

/// Build physics collision AABBs in local block space [0,1] from resolved model elements.
/// Applies the same element + variant transform stack as the mesh baker.
/// Degenerate (zero-thickness) elements are skipped so cross/plant planes stay non-solid.
std::vector<blockstate::CollisionAabb> BuildCollisionBoxes(
    const ResolvedBlockModelDefinition& resolved,
    int variantX,
    int variantY
);

} // namespace blockmodels
