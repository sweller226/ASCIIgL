#pragma once

namespace ecs::components {

/// Tag component to mark entities as item definitions (prototypes).
/// These entities hold shared data for an item type (e.g., "diamond sword")
/// and should NOT be processed by gameplay systems.
struct ItemDefinitionTag {};

} // namespace ecs::components
