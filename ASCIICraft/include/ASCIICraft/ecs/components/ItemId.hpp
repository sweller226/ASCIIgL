#pragma once

#include <string>

namespace ecs::components {

/// Identity component for item definition entities.
/// Replaces ItemDefinition::id, name, displayName.
struct ItemId {
    int numericId = -1;            // Unique numeric ID (e.g., 1 for stone)
    std::string registryName;      // Registry name (e.g., "minecraft:stone")
    std::string displayName;       // Human-readable name (e.g., "Stone")
};

} // namespace ecs::components
