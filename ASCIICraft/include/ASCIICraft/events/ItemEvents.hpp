#pragma once

#include <glm/vec3.hpp>
#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/Inventory.hpp>

namespace events {

/// Emitted when a player picks up a dropped item
struct ItemPickupEvent {
    entt::entity player;        // Entity picking up
    entt::entity droppedItem;   // The dropped item entity (will be destroyed)
    ecs::components::ItemStack itemStack;
};

/// Emitted when an entity wants to drop an item from inventory
struct ItemDropEvent {
    entt::entity dropper;       // Entity dropping the item
    ecs::components::ItemStack itemStack;
    glm::vec3 position;
    glm::vec3 velocity;
};

/// Emitted when an inventory slot changes (for UI updates)
struct InventoryChangedEvent {
    entt::entity owner;         // Entity whose inventory changed
    int slotIndex;
    ecs::components::ItemStack oldStack;
    ecs::components::ItemStack newStack;
};

} // namespace events
