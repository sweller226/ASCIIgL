#pragma once

#include <string>

#include <ASCIICraft/util/JsonUtil.hpp>
#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>
#include <ASCIICraft/world/block/models/ResolvedBlockModel.hpp>

namespace blockmodels {

    // Loads parsed models via loader, walks parent chain with cycle detection, merges textures
    // (child overrides parent), inherits elements when child has none, resolves face #texture refs.
    jsonutil::LoadResult<ResolvedBlockModelDefinition> ResolveBlockModel(JsonModelLoader& loader, const std::string& resourceId);

} // namespace blockmodels
