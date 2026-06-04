#pragma once

#include <entt/entt.hpp>

namespace gui {

/// Updated each frame by GUIManager while a blocking GUI screen is active (inventory open).
/// Read by InventorySystem when handling DropItemPressedEvent.
struct GuiSlotHover {
    entt::entity inventoryOwner = entt::null;
    int slotIndex = -1;

    bool IsValid() const {
        return inventoryOwner != entt::null && slotIndex >= 0;
    }
};

} // namespace gui
