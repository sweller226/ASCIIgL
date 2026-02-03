#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

// Forward declarations
class EventBus;

namespace ecs::systems {

/// System for managing GUI layout and cursor-based navigation.
/// Uses arrow keys to move a virtual cursor, interact actions to activate elements.
class GUISystem : public ISystem {
public:
    GUISystem(entt::registry& registry, EventBus& eventBus);
    void Update() override;

    /// Set the screen dimensions for layout calculations
    void SetScreenSize(float width, float height);

    /// Create the cursor entity (call once at startup)
    entt::entity CreateCursor();

    /// Get the cursor entity
    entt::entity GetCursorEntity() const { return cursorEntity; }

    /// Open a panel by ID
    void OpenPanel(const std::string& panelId);

    /// Close a panel by ID
    void ClosePanel(const std::string& panelId);

    /// Toggle a panel by ID
    void TogglePanel(const std::string& panelId);

    /// Check if any panel is blocking game input
    bool IsBlockingInput() const;

private:
    entt::registry& registry;
    EventBus& eventBus;

    glm::vec2 screenSize{1920.0f, 1080.0f};
    entt::entity cursorEntity = entt::null;

    void UpdateLayout();
    void ProcessCursorInput();
    void UpdateHoveredElement();
    void SyncInventorySlots();

    /// Calculate screen position from anchor and offset
    glm::vec2 CalculateScreenPosition(const glm::vec2& anchor, 
                                       const glm::vec2& pivot,
                                       const glm::vec2& offset,
                                       const glm::vec2& size) const;

    /// Check AABB collision between cursor and element
    bool CursorIntersectsElement(const glm::vec2& cursorPos,
                                  const glm::vec2& cursorSize,
                                  const glm::vec2& elementPos, 
                                  const glm::vec2& elementSize) const;

    /// Find topmost interactable element under cursor
    entt::entity FindElementUnderCursor();
};

} // namespace ecs::systems
