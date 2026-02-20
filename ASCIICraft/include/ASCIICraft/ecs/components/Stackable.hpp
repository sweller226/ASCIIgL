#pragma once

namespace ecs::components {

/// Component indicating an item can stack in inventory.
/// Items without this component are implicitly unstackable (stack size = 1).
struct Stackable {
    int maxStackSize = 64;
};

} // namespace ecs::components
