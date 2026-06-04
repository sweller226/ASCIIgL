#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/ItemProperties.hpp>

namespace ASCIIgL { class Mesh; }

namespace ecs::data {

/// Registry of item definition entities (id/name → prototype).
/// Stored in registry.ctx() alongside BlockStateRegistry.
class ItemRegistry {
public:
    /// Register a definition entity. Returns false if ID or name already exists.
    bool Register(int id, const std::string& name, entt::entity entity);

    /// Register a definition entity using the next auto-incremented ID.
    /// Returns the assigned ID, or -1 on failure.
    int RegisterNext(const std::string& name, entt::entity entity);

    /// Look up definition entity by numeric ID.
    /// Returns entt::null if not found.
    entt::entity Resolve(int id) const;

    /// Look up definition entity by registry name (e.g., "minecraft:diamond").
    /// Returns entt::null if not found.
    entt::entity Resolve(const std::string& name) const;

    /// Check if a numeric ID is registered.
    bool Exists(int id) const;

    /// Registry name for a numeric id (e.g. "minecraft:stone"). Empty if unknown.
    std::string GetNameFromId(int id, entt::registry& reg) const;

    /// Numeric id for a registry name. -1 if unknown.
    int GetIdFromName(const std::string& name, entt::registry& reg) const;

    /// Clear all registrations.
    void Clear();

    // ========================================================================
    // Item Definition Builders
    // ========================================================================

    /// Register a placeable block item (auto-incrementing item id).
    entt::entity RegisterBlockItem(
        entt::registry& reg,
        const std::string& name, const std::string& display,
        int maxStack = 64
    );

    /// Register a stackable resource/material item (2D icon, auto-incrementing item id).
    entt::entity RegisterResourceItem(
        entt::registry& reg,
        const std::string& name, const std::string& display,
        float iconLayer, int maxStack = 64
    );

    /// Register a tool/weapon item (2D icon, auto-incrementing item id).
    entt::entity RegisterToolItem(
        entt::registry& reg,
        const std::string& name, const std::string& display,
        float iconLayer,
        ecs::components::ToolProperty tool, ecs::components::WeaponProperty weapon
    );

    /// Get the current next ID value (useful for inspecting the counter).
    int GetNextId() const { return nextId; }

    /// Manually set the next ID counter (e.g., to skip past block IDs for non-block items).
    void SetNextId(int id) { nextId = id; }

private:
    std::shared_ptr<ASCIIgL::Mesh> buildIconMesh(float iconLayer) const;
    std::shared_ptr<ASCIIgL::Mesh> buildBlockItemMesh(
        entt::registry& reg,
        entt::entity definitionEntity
    ) const;

    int nextId = 1;  // Auto-increment counter, starts at 1
    std::unordered_map<int, entt::entity> byId;
    std::unordered_map<std::string, entt::entity> byName;
};

} // namespace ecs::data
