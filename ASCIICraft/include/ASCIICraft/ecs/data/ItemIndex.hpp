#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/ItemProperties.hpp>

namespace ASCIIgL { class Mesh; }
namespace blockstate { struct BlockState; }

namespace ecs::data {

/// Thin O(1) lookup index for item definition entities.
/// Stored in registry.ctx() — replaces the ItemRegistry singleton.
class ItemIndex {
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

    /// Clear all registrations.
    void Clear();

    // ========================================================================
    // Item Definition Builders
    // ========================================================================

    /// Register a placeable block item (auto-incrementing ID = blockId).
    entt::entity RegisterBlockItem(
        entt::registry& reg,
        const std::string& name, const std::string& display,
        std::shared_ptr<ASCIIgL::Mesh> mesh, int maxStack = 64
    );

    /// Register a stackable resource/material item (2D icon).
    entt::entity RegisterResourceItem(
        entt::registry& reg,
        int id, const std::string& name, const std::string& display,
        std::shared_ptr<ASCIIgL::Mesh> mesh, int maxStack = 64
    );

    /// Register a tool/weapon item (unstackable, 2D icon).
    entt::entity RegisterToolItem(
        entt::registry& reg,
        int id, const std::string& name, const std::string& display,
        std::shared_ptr<ASCIIgL::Mesh> mesh,
        ecs::components::ToolProperty tool, ecs::components::WeaponProperty weapon
    );

    // ========================================================================
    // Mesh Utilities
    // ========================================================================

    /// Create a quad mesh for a 2D item icon from a texture array layer ID.
    static std::shared_ptr<ASCIIgL::Mesh> GetQuadItemMesh(int layerID);

    /// Create a quad mesh for a 2D item icon from atlas grid coordinates.
    static std::shared_ptr<ASCIIgL::Mesh> GetQuadItemMesh(int x, int y, int atlas_size = 16);

    /// Create a 3D block-preview cube mesh from a BSR BlockState's face texture layers.
    static std::shared_ptr<ASCIIgL::Mesh> GetBlockMeshFromState(const blockstate::BlockState& state);

    /// Get the current next ID value (useful for inspecting the counter).
    int GetNextId() const { return nextId; }

    /// Manually set the next ID counter (e.g., to skip past block IDs for non-block items).
    void SetNextId(int id) { nextId = id; }

private:
    int nextId = 1;  // Auto-increment counter, starts at 1
    std::unordered_map<int, entt::entity> byId;
    std::unordered_map<std::string, entt::entity> byName;
};

} // namespace ecs::data
