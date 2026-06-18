#pragma once

#include <glm/vec3.hpp>
#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/Inventory.hpp>

namespace events {

/// Emitted when the player presses the key bound to "drop_item" (e.g. Q).
/// Emitted regardless of input mode; InventorySystem drops one item (carried / hover / hotbar).
struct DropItemPressedEvent {};

/// Emitted when the player presses interact_left (F) in GUI with no slot under the cursor.
/// InventorySystem drops the full ItemCarried stack if any.
struct DropCarriedStackPressedEvent {};

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
    /// If >= 0, remove this many from inventory slot index (itemStack.count; -1 = all in slot via removeItemAt).
    int slotIndex = -1;
    /// Remove itemStack.count from ItemCarried; processDropEvents applies removal and spawns.
    bool fromCarried = false;
};

/// Emitted when an inventory slot changes (for UI updates)
struct InventoryChangedEvent {
    entt::entity owner;         // Entity whose inventory changed
    int slotIndex;
    ecs::components::ItemStack oldStack;
    ecs::components::ItemStack newStack;
};

/// Emitted when the mouse-held stack changes during inventory GUI interaction
struct ItemCarriedChangedEvent {
    entt::entity owner;
    ecs::components::ItemStack oldStack;
    ecs::components::ItemStack newStack;
};

} // namespace events
