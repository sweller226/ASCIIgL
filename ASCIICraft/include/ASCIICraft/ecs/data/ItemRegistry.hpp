#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <any>

#include <ASCIIgL/engine/Mesh.hpp>

#include <ASCIICraft/world/Block.hpp>

namespace ecs::data {

// ============================================================================
// Common Item Properties (extend as needed)
// ============================================================================

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
    bool canEatWhenFull = false; // Like golden apple
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

// ============================================================================
// Item Definition
// ============================================================================

/// Definition of an item type (not an instance).
/// Registered once, shared by all ItemStacks with the same itemId.
struct ItemDefinition {
    int id = -1;                          // Unique numeric ID
    std::string name;                     // Registry name (e.g., "minecraft:diamond")
    std::string displayName;              // Human-readable name (e.g., "Diamond")
    
    int maxStackSize = 64;                // Max items per stack
    bool isStackable = true;              // Can this item stack at all?
    
    // Visuals
    std::shared_ptr<ASCIIgL::Mesh> mesh; // PosUVLayer
    bool is2DIcon = false;                // If true, use icon texture array instead of block texture array

    // ========================================================================
    // ECS-Style Property System
    // ========================================================================

private:
    std::unordered_map<std::type_index, std::any> properties;

public:
    /// Add a property to this item definition
    template<typename T>
    void set(const T& property) {
        properties[std::type_index(typeid(T))] = property;
    }

    /// Get a property (returns nullptr if not present)
    template<typename T>
    const T* get() const {
        auto it = properties.find(std::type_index(typeid(T)));
        if (it != properties.end()) {
            return std::any_cast<T>(&it->second);
        }
        return nullptr;
    }

    /// Check if this item has a specific property
    template<typename T>
    bool has() const {
        return properties.find(std::type_index(typeid(T))) != properties.end();
    }

    /// Remove a property
    template<typename T>
    void remove() {
        properties.erase(std::type_index(typeid(T)));
    }
};

// ============================================================================
// Item Registry (Singleton)
// ============================================================================

/// Singleton registry for all item definitions.
/// Register items at startup, then look them up by ID or name.
class ItemRegistry {
public:
    static ItemRegistry& Instance();

    // Prevent copying
    ItemRegistry(const ItemRegistry&) = delete;
    ItemRegistry& operator=(const ItemRegistry&) = delete;

    /// Register a new item definition. Returns false if ID or name already exists.
    bool RegisterItem(const ItemDefinition& def);

    /// Look up by numeric ID
    const ItemDefinition* getById(int id) const;

    /// Look up by registry name (e.g., "minecraft:diamond")
    const ItemDefinition* getByName(const std::string& name) const;

    /// Check if an ID is registered
    bool exists(int id) const;

    /// Get all registered items (for iteration)
    const std::unordered_map<int, ItemDefinition>& getAll() const { return itemsById; }

    /// Clear all registrations (useful for reloading)
    void clear();

    static std::shared_ptr<ASCIIgL::Mesh> GetQuadItemMesh(int layerID);
    static std::shared_ptr<ASCIIgL::Mesh> GetQuadItemMesh(int x, int y, int atlas_size = 16);

    static std::string MakeItemName(const std::string& name);
    static ItemDefinition MakeItemDef(
        int id,
        const std::string& name,
        const std::string& displayName,
        int maxStackSize,
        bool isStackable,
        std::shared_ptr<ASCIIgL::Mesh> mesh,
        bool is2DIcon = false
    );

private:
    ItemRegistry() = default;

    std::unordered_map<int, ItemDefinition> itemsById;
    std::unordered_map<std::string, int> nameToId;
};

} // namespace ecs::data

