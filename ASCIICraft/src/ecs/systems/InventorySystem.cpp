#include <ASCIICraft/ecs/systems/InventorySystem.hpp>
#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/events/ItemEvents.hpp>
#include <ASCIICraft/ecs/factories/ItemFactory.hpp>

namespace ecs::systems {

using namespace ecs::components;

InventorySystem::InventorySystem(entt::registry& registry, EventBus& eventBus) 
    : registry(registry), eventBus(eventBus) {}

void InventorySystem::Update() {
    processPickupEvents();
    processDropEvents();
}

// ============================================================================
// Static Helpers
// ============================================================================

int InventorySystem::findSlotFor(const Inventory& inv, int itemId) {
    // First pass: find existing stack with room
    for (int i = 0; i < inv.capacity; ++i) {
        if (inv.slots[i].itemId == itemId && inv.slots[i].count < inv.slots[i].maxStackSize) {
            return i;
        }
    }
    // Second pass: find empty slot
    for (int i = 0; i < inv.capacity; ++i) {
        if (inv.slots[i].isEmpty()) {
            return i;
        }
    }
    return -1;
}

int InventorySystem::addToStack(ItemStack& stack, int amount) {
    int canFit = stack.maxStackSize - stack.count;
    int toAdd = (amount < canFit) ? amount : canFit;
    stack.count += toAdd;
    return amount - toAdd; // overflow
}

int InventorySystem::removeFromStack(ItemStack& stack, int amount) {
    int toRemove = (amount < stack.count) ? amount : stack.count;
    stack.count -= toRemove;
    if (stack.count <= 0) {
        stack.itemId = -1;
        stack.count = 0;
    }
    return toRemove;
}

ItemStack InventorySystem::addItem(Inventory& inv, const ItemStack& item) {
    ItemStack remaining = item;

    while (remaining.count > 0) {
        int slotIdx = findSlotFor(inv, remaining.itemId);
        if (slotIdx < 0) break; // No room

        ItemStack& slot = inv.slots[slotIdx];
        if (slot.isEmpty()) {
            slot.itemId = remaining.itemId;
            slot.maxStackSize = remaining.maxStackSize;
            slot.metadata = remaining.metadata;
        }
        remaining.count = addToStack(slot, remaining.count);
    }

    return remaining;
}

ItemStack InventorySystem::removeItemAt(Inventory& inv, int slotIndex, int count) {
    if (slotIndex < 0 || slotIndex >= inv.capacity) {
        return ItemStack{}; // Invalid slot
    }

    ItemStack& slot = inv.slots[slotIndex];
    if (slot.isEmpty()) {
        return ItemStack{}; // Nothing to remove
    }

    ItemStack removed;
    removed.itemId = slot.itemId;
    removed.maxStackSize = slot.maxStackSize;
    removed.metadata = slot.metadata;

    if (count < 0 || count >= slot.count) {
        // Remove all
        removed.count = slot.count;
        slot = ItemStack{}; // Clear slot
    } else {
        // Remove partial
        removed.count = count;
        slot.count -= count;
    }

    return removed;
}

ItemStack InventorySystem::removeItem(Inventory& inv, int itemId, int count) {
    ItemStack removed;
    removed.itemId = itemId;
    removed.count = 0;

    int remaining = count;
    for (int i = 0; i < inv.capacity && remaining > 0; ++i) {
        if (inv.slots[i].itemId == itemId) {
            int toRemove = (inv.slots[i].count < remaining) ? inv.slots[i].count : remaining;
            inv.slots[i].count -= toRemove;
            remaining -= toRemove;
            removed.count += toRemove;

            if (removed.maxStackSize == 64) {
                removed.maxStackSize = inv.slots[i].maxStackSize;
                removed.metadata = inv.slots[i].metadata;
            }

            if (inv.slots[i].count <= 0) {
                inv.slots[i] = ItemStack{}; // Clear slot
            }
        }
    }

    if (removed.count == 0) {
        removed.itemId = -1; // Nothing removed
    }

    return removed;
}

// ============================================================================
// Event Processing
// ============================================================================

void InventorySystem::processPickupEvents() {
    for (const auto& event : eventBus.view<events::ItemPickupEvent>()) {
        auto* inv = registry.try_get<Inventory>(event.player);
        if (!inv) continue;

        ItemStack overflow = addItem(*inv, event.itemStack);

        // Emit change event for UI
        eventBus.emit(events::InventoryChangedEvent{
            event.player,
            -1, // Multiple slots may have changed
            ItemStack{}, // Old state unknown
            event.itemStack
        });

        // If fully picked up, mark the dropped item for destruction
        if (overflow.isEmpty()) {
            if (registry.valid(event.droppedItem)) {
                registry.destroy(event.droppedItem);
            }
        } else {
            // Update the dropped item with remaining amount
            auto* stack = registry.try_get<ItemStack>(event.droppedItem);
            if (stack) {
                stack->count = overflow.count;
            }
        }
    }
}

void InventorySystem::processDropEvents() {
    ecs::factories::ItemFactory factory(registry);

    for (const auto& event : eventBus.view<events::ItemDropEvent>()) {
        factory.createDroppedItem(
            event.itemStack,
            event.position,
            event.velocity
        );

        // Emit change event for UI
        eventBus.emit(events::InventoryChangedEvent{
            event.dropper,
            -1,
            event.itemStack,
            ItemStack{}
        });
    }
}

} // namespace ecs::systems