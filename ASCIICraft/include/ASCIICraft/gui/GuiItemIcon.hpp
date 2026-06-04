#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>

namespace ecs::components { struct ItemStack; }

namespace gui {

class GUIRenderer;

/// Minecraft draws a 16x16 item icon inside the slot cell (hotbar cells are ~20px).
inline constexpr float kItemIconPixels = 16.0f;

/// Draws \p stack centered in a GUI cell. Returns false if nothing was drawn.
bool DrawItemStackIcon(entt::registry& registry,
                      GUIRenderer& renderer,
                      const ecs::components::ItemStack& stack,
                      glm::vec2 cellTopLeftPx,
                      glm::vec2 cellSizePx,
                      int layer);

} // namespace gui
