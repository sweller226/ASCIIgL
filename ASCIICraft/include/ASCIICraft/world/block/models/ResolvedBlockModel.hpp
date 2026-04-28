#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>

namespace blockmodels {

    // Resolved face definition used by the baker input path.
    // texture must be a concrete texture path/resource id (never "#var").
    struct ResolvedBlockModelFace {
        std::optional<std::array<float, 4>> uv;
        std::string texture;
        std::optional<std::string> cullface;
        int rotation = 0;
    };

    struct ResolvedBlockModelElement {
        std::array<float, 3> from = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> to = { 16.0f, 16.0f, 16.0f };
        std::optional<BlockModelElementRotationDef> rotation;
        std::optional<bool> shade;
        std::unordered_map<std::string, ResolvedBlockModelFace> faces;
    };

    // Fully resolved model definition:
    // - no parent
    // - element face textures are concrete (no "#...")
    struct ResolvedBlockModelDefinition {
        bool isFullBlock = false;
        bool opaqueNoCull = false;
        std::vector<ResolvedBlockModelElement> elements;
    };

} // namespace blockmodels

