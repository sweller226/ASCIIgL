#include <ASCIICraft/ecs/factories/gui/InventoryGUIFactory.hpp>

namespace ecs::factories::gui {

entt::entity InventoryGUIFactory::Create(entt::registry& registry, entt::entity owner) {
    // Static Minecraft inventory layout: 9 columns x 4 rows
    constexpr int columns    = 9;
    constexpr int rows       = 4;
    constexpr float slotSize    = 32.0f;
    constexpr float slotSpacing = 4.0f;
    constexpr float padding     = 16.0f;

    // Calculate panel size based on grid
    constexpr float panelWidth  = padding * 2 + columns * slotSize + (columns - 1) * slotSpacing;
    constexpr float panelHeight = padding * 2 + rows * slotSize + (rows - 1) * slotSpacing;

    // Create root panel using base class helper
    entt::entity panel = CreatePanelEntity(
        registry,
        PANEL_ID,
        false,      // isOpen
        true,       // blocksInput
        true,       // closeOnEscape
        0.5f, 0.5f, // anchor (center)
        0.0f, 0.0f, // offset
        panelWidth, panelHeight,
        100         // layer
    );

    // Create slot entities using base class helper
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            int slotIndex = row * columns + col;

            // Calculate offset from panel center
            float startX = -panelWidth / 2.0f + padding + slotSize / 2.0f;
            float startY = -panelHeight / 2.0f + padding + slotSize / 2.0f;

            float offsetX = startX + col * (slotSize + slotSpacing);
            float offsetY = startY + row * (slotSize + slotSpacing);

            CreateSlotEntity(
                registry,
                panel,          // parentPanel
                slotIndex,
                owner,
                0.5f, 0.5f,     // anchor
                offsetX, offsetY,
                slotSize,
                101             // layer (above panel)
            );
        }
    }

    return panel;
}

void InventoryGUIFactory::Destroy(entt::registry& registry, entt::entity panel) {
    if (!registry.valid(panel)) return;
    
    // Destroy all child elements of the panel
    DestroyChildren(registry, panel);
    
    // Destroy the panel itself
    registry.destroy(panel);
}

} // namespace ecs::factories::gui
