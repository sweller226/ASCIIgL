#include <ASCIICraft/ecs/factories/gui/InventoryGUIFactory.hpp>

namespace ecs::factories::gui {

entt::entity InventoryGUIFactory::Create(entt::registry& registry, entt::entity owner) {
    return Create(registry, owner, Config{});
}

entt::entity InventoryGUIFactory::Create(
    entt::registry& registry,
    entt::entity inventoryOwner,
    const Config& config
) {
    currentConfig = config;
    
    // Calculate panel size based on grid
    float panelWidth = config.padding * 2 + 
                       config.columns * config.slotSize + 
                       (config.columns - 1) * config.slotSpacing;
    
    float panelHeight = config.padding * 2 + 
                        config.rows * config.slotSize + 
                        (config.rows - 1) * config.slotSpacing;

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
    for (int row = 0; row < config.rows; ++row) {
        for (int col = 0; col < config.columns; ++col) {
            int slotIndex = row * config.columns + col;
            
            // Calculate offset from panel center
            float startX = -panelWidth / 2.0f + config.padding + config.slotSize / 2.0f;
            float startY = -panelHeight / 2.0f + config.padding + config.slotSize / 2.0f;
            
            float offsetX = startX + col * (config.slotSize + config.slotSpacing);
            float offsetY = startY + row * (config.slotSize + config.slotSpacing);
            
            CreateSlotEntity(
                registry,
                panel,          // parentPanel
                slotIndex,
                inventoryOwner,
                0.5f, 0.5f,     // anchor
                offsetX, offsetY,
                config.slotSize,
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
