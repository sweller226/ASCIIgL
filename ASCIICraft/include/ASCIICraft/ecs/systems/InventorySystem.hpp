#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>

// Forward declarations
class EventBus;

namespace ecs::systems {

/// System for managing inventory operations and processing item events.
/// Contains static helpers for inventory manipulation.
class InventorySystem : public ISystem {
public:
    explicit InventorySystem(entt::registry& registry, EventBus& eventBus);
    void Update() override;

    // ========================================================================
    // Static Inventory Helpers
    // ========================================================================

    /// Finds the first slot with a matching itemId that has room, or an empty slot.
    /// Returns -1 if no suitable slot is found.
    static int findSlotFor(const ecs::components::Inventory& inv, int itemId);

    /// Adds items to a stack. Returns overflow count.
    static int addToStack(ecs::components::ItemStack& stack, int amount);

    /// Removes items from a stack. Returns actual removed count.
    static int removeFromStack(ecs::components::ItemStack& stack, int amount);

    /// Attempts to add an ItemStack to the inventory. Returns any overflow as a new ItemStack.
    static ecs::components::ItemStack addItem(ecs::components::Inventory& inv, const ecs::components::ItemStack& item);

    /// Removes items from a specific slot. Returns the removed ItemStack.
    /// @param count How many to remove (-1 = all)
    static ecs::components::ItemStack removeItemAt(ecs::components::Inventory& inv, int slotIndex, int count = -1);

    /// Removes items by item ID from anywhere in inventory. Returns the removed ItemStack.
    static ecs::components::ItemStack removeItem(ecs::components::Inventory& inv, int itemId, int count);

private:
    entt::registry& registry;
    EventBus& eventBus;

    void processPickupEvents();
    void processDropEvents();
};

} // namespace ecs::systems