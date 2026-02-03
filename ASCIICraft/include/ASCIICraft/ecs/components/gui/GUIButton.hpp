#pragma once

#include <string>

namespace ecs::components {

/// Represents a clickable button in the GUI.
/// Triggers an action when selected and interacted with.
struct GUIButton {
    std::string actionId;        // Unique action identifier (e.g., "settings.sound", "quit")
    std::string label;           // Display text for the button

    // Interaction state (updated by GUISystem)
    bool isHovered = false;
    bool isSelected = false;
    bool isDisabled = false;     // If true, cannot be interacted with
};

} // namespace ecs::components
