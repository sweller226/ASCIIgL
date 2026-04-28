#pragma once

#include <string>

#include <ASCIICraft/util/JsonUtil.hpp>
#include <ASCIICraft/world/block/models/BlockModel.hpp>
#include <ASCIICraft/world/block/models/ResolvedBlockModel.hpp>

namespace blockmodels {

    // M3A contract implementation:
    // - Validates variant transform payload.
    // - Produces a runtime block model object ready for later geometry population.
    // - Leaves computeVisibleFaces null for JSON-loaded models (current project policy).
    // Geometry emission (elements/faces -> vertices/indices) lands in later M3 phases.
    jsonutil::LoadResult<blockstate::BlockModel> BakeResolvedModel(
        const ResolvedBlockModelDefinition& resolved,
        int variantX,
        int variantY,
        bool variantUvlock
    );

} // namespace blockmodels

