#pragma once

namespace ecs::components {

/// Tag component to mark entities as dropped items in the world.
/// Used by DroppedItemSystem for querying.
struct DroppedItemTag {};

} // namespace ecs::components
