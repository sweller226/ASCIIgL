#pragma once

#include <functional>
#include <string>

#include <ASCIICraft/util/JsonUtil.hpp>
#include <ASCIICraft/world/blockmodels/BlockModel.hpp>
#include <ASCIICraft/world/blockmodels/ResolvedBlockModel.hpp>

namespace blockmodels {

    // Resolves a concrete texture resource id (e.g. "minecraft:blocks/stone")
    // to the engine texture-array layer index.
    using TextureLayerResolver = std::function<jsonutil::LoadResult<int>(const std::string&)>;

    // M3A contract implementation:
    // - Validates variant transform payload.
    // - Produces a runtime block model object ready for later geometry population.
    // - Leaves computeVisibleFaces null for JSON-loaded models (current project policy).
    // Geometry emission (elements/faces -> vertices/indices) lands in later M3 phases.
    jsonutil::LoadResult<blockstate::BlockModel> BakeResolvedModel(
        const ResolvedBlockModelDefinition& resolved,
        int variantX,
        int variantY,
        bool variantUvlock,
        const TextureLayerResolver& resolveTextureLayer
    );

} // namespace blockmodels

