#pragma once

#include <glm/vec2.hpp>
#include <entt/entt.hpp>

namespace ecs::components {

/// Core GUI layout component for positioning elements on screen.
/// Uses anchor-based positioning relative to screen or parent.
struct GUIElement {
    glm::vec2 anchor{0.5f, 0.5f};   // Normalized (0-1) screen position
    glm::vec2 pivot{0.5f, 0.5f};    // Pivot point within element (0-1)
    glm::vec2 offset{0.0f, 0.0f};   // Pixel offset from anchor
    glm::vec2 size{100.0f, 100.0f}; // Width/height in pixels

    int layer = 0;                   // Z-order (higher = on top)
    bool interactable = true;        // Can receive input events
    bool visible = true;             // Is rendered

    // Parent panel for hierarchy (entt::null = screen root)
    entt::entity parentPanel = entt::null;

    // Computed screen position (updated by GUISystem)
    glm::vec2 screenPosition{0.0f, 0.0f};
};

} // namespace ecs::components
