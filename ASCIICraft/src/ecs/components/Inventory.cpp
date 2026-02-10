#include <ASCIICraft/ecs/components/Inventory.hpp>

#include <ASCIICraft/ecs/data/ItemIndex.hpp>
#include <ASCIICraft/ecs/components/Stackable.hpp>

namespace ecs::components {

ItemStack ItemStack::fromRegistry(entt::registry& reg, int id, int count) {
    ItemStack stack;
    
    auto& itemIndex = reg.ctx().get<ecs::data::ItemIndex>();
    auto proto = itemIndex.Resolve(id);
    if (proto != entt::null) {
        auto* stackable = reg.try_get<Stackable>(proto);
        stack.itemId = id;
        stack.count = count;
        stack.maxStackSize = stackable ? stackable->maxStackSize : 1;
    }
    // If not found, returns empty stack (itemId = -1)
    
    return stack;
}

} // namespace ecs::components
