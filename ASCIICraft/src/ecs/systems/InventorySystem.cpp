#include <ASCIICraft/ecs/systems/InventorySystem.hpp>
#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/events/ItemEvents.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>
#include <ASCIICraft/ecs/components/ItemCarried.hpp>
#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/factories/ItemFactory.hpp>
#include <ASCIICraft/gui/GuiSlotHover.hpp>

#include <algorithm>
#include <unordered_set>

namespace ecs::systems {

using namespace ecs::components;

InventorySystem::InventorySystem(entt::registry& registry, ASCIIgL::EventBus& eventBus) 
    : registry(registry), eventBus(eventBus) {}

void InventorySystem::Update() {
    processDropPressed();
    processDropCarriedStackPressed();
    processPickupEvents();
    processDropEvents();
    processSlotClickedEvents();
}

namespace {

bool stackChanged(const ItemStack& before, const ItemStack& after) {
    return before.itemId != after.itemId || before.count != after.count;
}

ItemStack oneItemFrom(const ItemStack& src) {
    ItemStack single = src;
    single.count = 1;
    return single;
}

bool emitCarriedDropIntent(
    ASCIIgL::EventBus& eventBus,
    entt::entity player,
    const ItemStack& carriedStack,
    const glm::vec3& dropPos,
    const glm::vec3& dropVel,
    int dropCount
) {
    if (carriedStack.isEmpty() || dropCount <= 0) {
        return false;
    }

    ItemStack intent = carriedStack;
    intent.count = std::min(dropCount, carriedStack.count);

    events::ItemDropEvent dropEvent{};
    dropEvent.dropper = player;
    dropEvent.position = dropPos;
    dropEvent.velocity = dropVel;
    dropEvent.itemStack = intent;
    dropEvent.fromCarried = true;
    eventBus.emit(dropEvent);
    return true;
}

bool computePlayerDropSpawn(entt::registry& registry, entt::entity player, glm::vec3& outPos, glm::vec3& outVel) {
    auto [pos, ok] = components::GetPos(player, registry);
    if (!ok) {
        return false;
    }

    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    if (auto* head = registry.try_get<Head>(player)) {
        forward = head->lookDir;
        pos = pos + head->relativePos;
    }

    outPos = pos + forward * 0.6f + glm::vec3(0.0f, 0.25f, 0.0f);
    outVel = forward * 3.0f;
    return true;
}

void handleLeftClick(ItemStack& carried, ItemStack& slot) {
    if (carried.isEmpty() && !slot.isEmpty()) {
        carried = slot;
        slot = ItemStack{};
        return;
    }
    if (!carried.isEmpty() && slot.isEmpty()) {
        slot = carried;
        carried = ItemStack{};
        return;
    }
    if (!carried.isEmpty() && !slot.isEmpty()) {
        if (carried.itemId == slot.itemId) {
            const int overflow = InventorySystem::addToStack(slot, carried.count);
            if (overflow == 0) {
                carried = ItemStack{};
            } else {
                carried.count = overflow;
            }
        } else {
            ItemStack temp = slot;
            slot = carried;
            carried = temp;
        }
    }
}

void handleRightClick(ItemStack& carried, ItemStack& slot) {
    if (carried.isEmpty() && !slot.isEmpty()) {
        const int half = (slot.count + 1) / 2;
        carried.itemId = slot.itemId;
        carried.count = half;
        carried.maxStackSize = slot.maxStackSize;
        carried.metadata = slot.metadata;
        InventorySystem::removeFromStack(slot, half);
        return;
    }
    if (!carried.isEmpty() && slot.isEmpty()) {
        slot.itemId = carried.itemId;
        slot.count = 1;
        slot.maxStackSize = carried.maxStackSize;
        slot.metadata = carried.metadata;
        InventorySystem::removeFromStack(carried, 1);
        return;
    }
    if (!carried.isEmpty() && !slot.isEmpty() && carried.itemId == slot.itemId) {
        const int overflow = InventorySystem::addToStack(slot, 1);
        if (overflow == 0) {
            InventorySystem::removeFromStack(carried, 1);
        }
    }
}

} // namespace

// ============================================================================
// Static Helpers
// ============================================================================

int InventorySystem::findSlotFor(const Inventory& inv, int itemId) {
    // First pass: find existing stack with room (skip corrupt count <= 0 slots)
    for (int i = 0; i < inv.capacity; ++i) {
        const ItemStack& slot = inv.slots[i];
        if (!slot.isEmpty() && slot.itemId == itemId && slot.count < slot.maxStackSize) {
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
    bool copiedProps = false;
    for (int i = 0; i < inv.capacity && remaining > 0; ++i) {
        if (inv.slots[i].itemId == itemId) {
            int toRemove = (inv.slots[i].count < remaining) ? inv.slots[i].count : remaining;
            inv.slots[i].count -= toRemove;
            remaining -= toRemove;
            removed.count += toRemove;

            if (!copiedProps) {
                removed.maxStackSize = inv.slots[i].maxStackSize;
                removed.metadata = inv.slots[i].metadata;
                copiedProps = true;
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
    std::unordered_set<entt::entity> handledDroppedItems;

    for (const auto& event : eventBus.view<events::ItemPickupEvent>()) {
        if (event.itemStack.isEmpty()
            || !registry.valid(event.player)
            || !registry.valid(event.droppedItem)) {
            continue;
        }

        // Same entity may be referenced by multiple events in one frame (e.g. after destroy).
        if (!handledDroppedItems.insert(event.droppedItem).second) {
            continue;
        }

        auto* inv = registry.try_get<Inventory>(event.player);
        if (!inv) continue;

        const std::vector<ItemStack> before = inv->slots;
        const ItemStack overflow = addItem(*inv, event.itemStack);

        for (int i = 0; i < inv->capacity; ++i) {
            if (before[i].itemId != inv->slots[i].itemId || before[i].count != inv->slots[i].count) {
                eventBus.emit(events::InventoryChangedEvent{
                    event.player, i, before[i], inv->slots[i]
                });
            }
        }

        if (!registry.valid(event.droppedItem)) {
            continue;
        }

        if (overflow.isEmpty()) {
            registry.destroy(event.droppedItem);
        } else {
            auto* stack = registry.try_get<ItemStack>(event.droppedItem);
            if (stack) {
                stack->count = overflow.count;
            }
        }
    }
}

void InventorySystem::processDropPressed() {
    for ([[maybe_unused]] const auto& pressed : eventBus.view<events::DropItemPressedEvent>()) {
        const entt::entity player = components::GetPlayerEntity(registry);
        if (player == entt::null || !registry.valid(player)) {
            continue;
        }

        auto* inv = registry.try_get<Inventory>(player);
        if (!inv) {
            continue;
        }

        glm::vec3 dropPos{};
        glm::vec3 dropVel{};
        if (!computePlayerDropSpawn(registry, player, dropPos, dropVel)) {
            continue;
        }

        events::ItemDropEvent dropEvent{};
        dropEvent.dropper = player;
        dropEvent.position = dropPos;
        dropEvent.velocity = dropVel;

        if (const auto* carried = registry.try_get<ItemCarried>(player)) {
            if (emitCarriedDropIntent(eventBus, player, carried->stack, dropPos, dropVel, 1)) {
                continue;
            }
        }

        if (const auto* hover = registry.ctx().find<gui::GuiSlotHover>()) {
            if (hover->IsValid() && hover->inventoryOwner == player
                && hover->slotIndex >= 0 && hover->slotIndex < inv->capacity) {
                const ItemStack& slot = inv->slots[hover->slotIndex];
                if (!slot.isEmpty()) {
                    dropEvent.slotIndex = hover->slotIndex;
                    dropEvent.itemStack = oneItemFrom(slot);
                    eventBus.emit(dropEvent);
                    continue;
                }
            }
        }

        auto* selection = registry.try_get<HotbarSelection>(player);
        if (!selection) {
            continue;
        }
        selection->clamp();
        const int hotbarSlot = selection->selectedSlot;
        if (hotbarSlot < 0 || hotbarSlot >= inv->capacity) {
            continue;
        }
        const ItemStack& hotbarStack = inv->slots[hotbarSlot];
        if (hotbarStack.isEmpty()) {
            continue;
        }

        dropEvent.slotIndex = hotbarSlot;
        dropEvent.itemStack = oneItemFrom(hotbarStack);
        eventBus.emit(dropEvent);
    }
}

void InventorySystem::processDropCarriedStackPressed() {
    for ([[maybe_unused]] const auto& pressed : eventBus.view<events::DropCarriedStackPressedEvent>()) {
        const entt::entity player = components::GetPlayerEntity(registry);
        if (player == entt::null || !registry.valid(player)) {
            continue;
        }

        auto* carried = registry.try_get<ItemCarried>(player);
        if (!carried || carried->stack.isEmpty()) {
            continue;
        }

        glm::vec3 dropPos{};
        glm::vec3 dropVel{};
        if (!computePlayerDropSpawn(registry, player, dropPos, dropVel)) {
            continue;
        }

        emitCarriedDropIntent(eventBus, player, carried->stack, dropPos, dropVel, carried->stack.count);
    }
}

void InventorySystem::processDropEvents() {
    ecs::factories::ItemFactory factory(registry);

    for (const auto& event : eventBus.view<events::ItemDropEvent>()) {
        if (!registry.valid(event.dropper)) {
            continue;
        }

        ItemStack removed;

        if (event.fromCarried) {
            auto* carried = registry.try_get<ItemCarried>(event.dropper);
            if (!carried || carried->stack.isEmpty() || event.itemStack.isEmpty()) {
                continue;
            }
            if (carried->stack.itemId != event.itemStack.itemId) {
                continue;
            }

            const int dropCount = std::min(event.itemStack.count, carried->stack.count);
            if (dropCount <= 0) {
                continue;
            }

            const ItemStack oldCarried = carried->stack;
            removed = carried->stack;
            removed.count = dropCount;

            if (dropCount >= carried->stack.count) {
                carried->stack = ItemStack{};
            } else {
                removeFromStack(carried->stack, dropCount);
            }

            eventBus.emit(events::ItemCarriedChangedEvent{
                event.dropper, oldCarried, carried->stack
            });
        } else {
            auto* inv = registry.try_get<Inventory>(event.dropper);
            if (!inv) {
                continue;
            }

            if (event.slotIndex >= 0) {
                if (event.slotIndex >= inv->capacity) {
                    continue;
                }
                const ItemStack oldSlot = inv->slots[event.slotIndex];
                if (oldSlot.isEmpty()) {
                    continue;
                }
                const int removeCount = event.itemStack.count > 0 ? event.itemStack.count : -1;
                removed = removeItemAt(*inv, event.slotIndex, removeCount);
                if (!removed.isEmpty()) {
                    eventBus.emit(events::InventoryChangedEvent{
                        event.dropper, event.slotIndex, oldSlot, inv->slots[event.slotIndex]
                    });
                }
            } else {
                if (event.itemStack.isEmpty()) {
                    continue;
                }
                removed = removeItem(*inv, event.itemStack.itemId, event.itemStack.count);
                if (!removed.isEmpty()) {
                    eventBus.emit(events::InventoryChangedEvent{event.dropper, -1, removed, ItemStack{}});
                }
            }
        }

        if (removed.isEmpty()) {
            continue;
        }

        factory.createDroppedItem(removed, event.position, event.velocity);
    }
}

void InventorySystem::processSlotClickedEvents() {
    for (const auto& event : eventBus.view<events::SlotClickedEvent>()) {
        if (event.inventoryOwner == entt::null || !registry.valid(event.inventoryOwner)) {
            continue;
        }

        auto* inv = registry.try_get<Inventory>(event.inventoryOwner);
        if (!inv || event.slotIndex < 0 || event.slotIndex >= inv->capacity) {
            continue;
        }

        auto& carried = registry.get_or_emplace<components::ItemCarried>(event.inventoryOwner).stack;
        ItemStack& slot = inv->slots[event.slotIndex];
        const ItemStack oldSlot = slot;
        const ItemStack oldCarried = carried;

        if (event.mouseButton == 0) {
            handleLeftClick(carried, slot);
        } else if (event.mouseButton == 1) {
            handleRightClick(carried, slot);
        }

        if (stackChanged(oldSlot, slot)) {
            eventBus.emit(events::InventoryChangedEvent{
                event.inventoryOwner, event.slotIndex, oldSlot, slot
            });
        }
        if (stackChanged(oldCarried, carried)) {
            eventBus.emit(events::ItemCarriedChangedEvent{
                event.inventoryOwner, oldCarried, carried
            });
        }
    }
}

} // namespace ecs::systems