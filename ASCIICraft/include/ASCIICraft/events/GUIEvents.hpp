#pragma once

#include <string>
#include <glm/vec2.hpp>
#include <entt/entt.hpp>

namespace events {

/// Emitted when a GUI slot is clicked (OOP or ECS). For OOP, slot is entt::null; use inventoryOwner + slotIndex.
struct SlotClickedEvent {
    entt::entity slot = entt::null;           // ECS slot entity, or null for OOP
    entt::entity inventoryOwner = entt::null;  // Entity with Inventory (required for OOP)
    int slotIndex = -1;
    int mouseButton = 0;  // 0 = left, 1 = right, 2 = middle
    bool shift = false;
};

/// Emitted when mouse hovers over a slot
struct SlotHoveredEvent {
    entt::entity slot;
    int slotIndex;
};

/// Emitted when mouse leaves a slot
struct SlotUnhoveredEvent {
    entt::entity slot;
    int slotIndex;
};

/// Emitted when a GUI button is clicked
struct ButtonClickedEvent {
    entt::entity button;
    std::string actionId;
    int mouseButton;  // 0 = left, 1 = right
};

/// Emitted when a button is hovered/selected
struct ButtonHoveredEvent {
    entt::entity button;
    std::string actionId;
};

/// Emitted when a panel is opened
struct PanelOpenedEvent {
    std::string panelId;
    entt::entity panel;
};

/// Emitted when a panel is closed
struct PanelClosedEvent {
    std::string panelId;
    entt::entity panel;
};

/// Emitted when any GUI element is clicked
struct GUIClickEvent {
    entt::entity element;
    glm::vec2 clickPosition;
    int mouseButton;
};

} // namespace events

