#pragma once

namespace ecs::components {

/// Which hotbar slot (0–8) the player has selected. Indexes into Inventory::slots.
struct HotbarSelection {
    static constexpr int kSlotCount = 9;

    int selectedSlot = 0;

    void clamp() {
        if (selectedSlot < 0) {
            selectedSlot = 0;
        } else if (selectedSlot >= kSlotCount) {
            selectedSlot = kSlotCount - 1;
        }
    }
};

} // namespace ecs::components
