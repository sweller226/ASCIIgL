#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include <entt/entt.hpp>

#include <ASCIIgL/engine/Mesh.hpp>

namespace ecs::components {

/// Represents a single stack of items in an inventory slot.
/// -1 itemId indicates an empty slot.
struct ItemStack {
    int itemId = -1;           // Item type ID (-1 = empty)
    int count = 0;             // Number of items in this stack
    int maxStackSize = 64;     // Maximum items allowed in this stack

    // Arbitrary metadata: durability, enchantments, NBT-like data, etc.
    std::unordered_map<std::string, int> metadata;

    bool isEmpty() const { return itemId < 0 || count <= 0; }

    /// Create an ItemStack from a registered item definition.
    /// Automatically sets maxStackSize from the registry.
    /// Returns an empty stack if the item ID is not registered.
    static ItemStack fromRegistry(entt::registry& reg, int id, int count = 1);
};

/// Pure data inventory component holding multiple item slots.
/// Attach to players, chests, or any entity that can hold items.
/// All manipulation logic is in InventorySystem.
struct Inventory {
    std::vector<ItemStack> slots;
    int capacity;

    explicit Inventory(int cap = 36) : slots(cap), capacity(cap) {}
};

} // namespace ecs::components

