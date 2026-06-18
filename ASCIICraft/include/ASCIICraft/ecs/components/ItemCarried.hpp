#pragma once

#include <ASCIICraft/ecs/components/Inventory.hpp>

namespace ecs::components {

/// Stack held by the mouse during inventory GUI interactions (not a world entity).
struct ItemCarried {
    ItemStack stack;
};

} // namespace ecs::components
