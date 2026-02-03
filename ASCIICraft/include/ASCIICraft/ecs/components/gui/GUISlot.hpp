#pragma once

#include <entt/entt.hpp>

namespace ecs::components {

/// Represents an inventory slot in the GUI.
/// Links a visual GUI element to an Inventory component slot.
struct GUISlot {
    int slotIndex = -1;                        // Maps to Inventory::slots[index]
    entt::entity inventoryOwner = entt::null;  // Entity with Inventory component

    // Interaction state (updated by GUISystem)
    bool isHovered = false;
    bool isSelected = false;
    bool isDragSource = false;                 // Currently being dragged from
};

} // namespace ecs::components
