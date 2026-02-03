#pragma once

#include <string>
#include <entt/entt.hpp>

namespace ecs::components {

/// Represents a GUI panel/window that can be opened/closed.
struct GUIPanel {
    std::string panelId;                      // Unique identifier (e.g., "inventory")
    bool isOpen = false;                       // Current open state
    entt::entity parentPanel = entt::null;     // Optional parent for nested panels

    // Input handling
    bool blocksGameInput = true;               // When open, blocks game controls
    bool closeOnEscape = true;                 // ESC key closes this panel
};

} // namespace ecs::components
