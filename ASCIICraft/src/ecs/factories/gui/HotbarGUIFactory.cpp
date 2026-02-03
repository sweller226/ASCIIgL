#include <ASCIICraft/ecs/factories/gui/HotbarGUIFactory.hpp>

namespace ecs::factories::gui {

entt::entity HotbarGUIFactory::Create(entt::registry& registry, entt::entity owner) {
    return Create(registry, owner, Config{});
}

entt::entity HotbarGUIFactory::Create(
    entt::registry& registry,
    entt::entity inventoryOwner,
    const Config& config
) {
    currentConfig = config;
    
    // Calculate hotbar size
    float hotbarWidth = config.slots * config.slotSize + 
                        (config.slots - 1) * config.slotSpacing;
    float hotbarHeight = config.slotSize;

    // Create root hotbar using base class helper
    entt::entity hotbar = CreatePanelEntity(
        registry,
        PANEL_ID,
        true,       // isOpen (always visible)
        false,      // blocksInput
        false,      // closeOnEscape
        0.5f, 1.0f, // anchor (bottom center)
        0.0f, -config.bottomMargin,
        hotbarWidth, hotbarHeight,
        50          // layer
    );

    // Create slot entities using base class helper
    for (int i = 0; i < config.slots; ++i) {
        float startX = -hotbarWidth / 2.0f + config.slotSize / 2.0f;
        float offsetX = startX + i * (config.slotSize + config.slotSpacing);
        
        CreateSlotEntity(
            registry,
            hotbar,         // parentPanel
            i,              // slotIndex (0-8 for hotbar)
            inventoryOwner,
            0.5f, 1.0f,     // anchor (bottom center)
            offsetX, -config.bottomMargin,
            config.slotSize,
            51              // layer (above hotbar background)
        );
    }
    
    return hotbar;
}

void HotbarGUIFactory::Destroy(entt::registry& registry, entt::entity hotbar) {
    if (!registry.valid(hotbar)) return;
    
    // Destroy all child elements of the hotbar
    DestroyChildren(registry, hotbar);
    
    registry.destroy(hotbar);
}

} // namespace ecs::factories::gui
