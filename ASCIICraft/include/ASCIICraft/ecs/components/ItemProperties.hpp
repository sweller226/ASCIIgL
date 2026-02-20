#pragma once

namespace ecs::components {

// ============================================================================
// Item Property Components
// ============================================================================
// These were previously in ItemRegistry.hpp under ecs::registries.
// Now they are proper ECS components attached to item definition entities.

/// Property for tools (pickaxe, axe, shovel, etc.)
struct ToolProperty {
    float miningSpeed = 1.0f;    // Speed multiplier
    int harvestLevel = 0;        // 0=hand, 1=wood, 2=stone, 3=iron, 4=diamond
    int durability = 0;          // Max durability (0 = unbreakable)
};

/// Property for food items
struct FoodProperty {
    int nutrition = 0;           // Hunger points restored
    float saturation = 0.0f;     // Saturation modifier
    bool canEatWhenFull = false;  // Like golden apple
};

/// Property for armor
struct ArmorProperty {
    enum class Slot { Head, Chest, Legs, Feet };
    Slot slot = Slot::Chest;
    int defense = 0;             // Armor points
    int toughness = 0;           // Armor toughness
};

/// Property for weapons
struct WeaponProperty {
    float attackDamage = 1.0f;
    float attackSpeed = 1.0f;    // Attacks per second
};

/// Property for blocks that can be placed
struct PlaceableProperty {
    int blockId = -1;            // Which block to place
};

} // namespace ecs::components
