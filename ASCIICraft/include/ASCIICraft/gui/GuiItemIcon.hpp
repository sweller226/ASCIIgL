#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>

namespace ecs::components { struct ItemStack; }

namespace gui {

class GUIRenderer;

/// Minecraft draws a 16x16 item icon inside the slot cell (hotbar cells are ~20px).
inline constexpr float kItemIconPixels = 16.0f;

/// Stack count draws on a higher GUI layer than the item icon (layer + offset).
inline constexpr int kItemCountLayerOffset = 2;

struct ItemIconRect {
    glm::vec2 topLeft{0.0f};
    glm::vec2 size{0.0f};
};

ItemIconRect ComputeItemIconRect(glm::vec2 cellTopLeftPx, glm::vec2 cellSizePx);

/// Draws \p stack icon (centered in cell) and count at bottom-right (count hidden when <= 1).
bool DrawItemStackIcon(entt::registry& registry,
                       GUIRenderer& renderer,
                       const ecs::components::ItemStack& stack,
                       glm::vec2 cellTopLeftPx,
                       glm::vec2 cellSizePx,
                       int layer);

} // namespace gui
