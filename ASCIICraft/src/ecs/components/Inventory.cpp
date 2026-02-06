#include <ASCIICraft/ecs/components/Inventory.hpp>

#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

namespace ecs::components {

ItemStack ItemStack::fromRegistry(int id, int count) {
    ItemStack stack;
    
    const auto* def = ecs::data::ItemRegistry::Instance().getById(id);
    if (def) {
        stack.itemId = id;
        stack.count = count;
        stack.maxStackSize = def->maxStackSize;
    }
    // If not found, returns empty stack (itemId = -1)
    
    return stack;
}

} // namespace ecs::components
