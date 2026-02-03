#pragma once

#include <entt/entt.hpp>
#include <string>
#include <memory>

namespace ASCIIgL { class Mesh; }

namespace ecs::factories::gui {

/// Base class for all GUI panel factories.
/// Provides common interface and helper methods for creating GUI panels.
class GUIFactory {
public:
    virtual ~GUIFactory() = default;

    /// Get the unique panel ID for this factory type.
    virtual const char* GetPanelId() const = 0;

    /// Create the panel and all child entities.
    /// @param registry The ECS registry
    /// @param owner Entity to bind to (e.g., player for inventory)
    /// @return The root panel entity
    virtual entt::entity Create(entt::registry& registry, entt::entity owner) = 0;

    /// Destroy the panel and all child entities.
    virtual void Destroy(entt::registry& registry, entt::entity panel) = 0;

    /// Find this factory's panel in the registry.
    entt::entity Find(entt::registry& registry) const;

    /// Set shared quad mesh for all GUI elements (call once at init)
    static void SetSharedQuadMesh(std::shared_ptr<ASCIIgL::Mesh> mesh);

    /// Get shared quad mesh
    static std::shared_ptr<ASCIIgL::Mesh> GetSharedQuadMesh();

protected:
    /// Shared quad mesh for all GUI elements
    static std::shared_ptr<ASCIIgL::Mesh> s_sharedQuadMesh;

    /// Helper: Create a slot entity with standard components.
    /// @param parentPanel The panel entity this slot belongs to
    static entt::entity CreateSlotEntity(
        entt::registry& registry,
        entt::entity parentPanel,
        int slotIndex,
        entt::entity inventoryOwner,
        float anchorX, float anchorY,
        float offsetX, float offsetY,
        float slotSize,
        int layer
    );

    /// Helper: Create a panel entity with standard components.
    static entt::entity CreatePanelEntity(
        entt::registry& registry,
        const std::string& panelId,
        bool isOpen,
        bool blocksInput,
        bool closeOnEscape,
        float anchorX, float anchorY,
        float offsetX, float offsetY,
        float width, float height,
        int layer
    );

    /// Helper: Create a button entity with standard components.
    /// @param parentPanel The panel entity this button belongs to
    /// @param actionId Unique action identifier for event handling
    /// @param label Display text for the button
    static entt::entity CreateButtonEntity(
        entt::registry& registry,
        entt::entity parentPanel,
        const std::string& actionId,
        const std::string& label,
        float anchorX, float anchorY,
        float offsetX, float offsetY,
        float width, float height,
        int layer
    );

    /// Helper: Destroy all slots belonging to a specific inventory owner.
    static void DestroySlots(entt::registry& registry, entt::entity inventoryOwner);

    /// Helper: Destroy slots by index range.
    static void DestroySlotsInRange(entt::registry& registry, int startIndex, int endIndex);

    /// Helper: Destroy all buttons with a specific action ID prefix.
    static void DestroyButtonsWithPrefix(entt::registry& registry, const std::string& prefix);

    /// Helper: Destroy all child elements of a panel.
    static void DestroyChildren(entt::registry& registry, entt::entity parentPanel);
};

} // namespace ecs::factories::gui
