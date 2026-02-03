#pragma once

#include <glm/vec2.hpp>
#include <entt/entt.hpp>

namespace ecs::components {

/// Represents a virtual cursor for GUI navigation.
/// Moves with arrow keys and detects collisions with GUIElement bounds.
/// Should be added to a single cursor entity in the registry.
struct GUICursor {
    glm::vec2 position{0.0f, 0.0f};    // Current screen position (pixels)
    glm::vec2 size{4.0f, 4.0f};         // Cursor hitbox size for AABB checks
    float moveSpeed = 8.0f;             // Pixels per frame
    bool visible = true;
    
    // Currently hovered element
    entt::entity hoveredElement = entt::null;
    entt::entity previousHovered = entt::null;
};

} // namespace ecs::components
