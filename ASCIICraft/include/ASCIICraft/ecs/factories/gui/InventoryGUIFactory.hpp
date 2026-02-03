#pragma once

#include <ASCIICraft/ecs/factories/gui/GUIFactory.hpp>

namespace ecs::factories::gui {

/// Factory for creating the inventory panel GUI.
/// Creates a panel with 36 slots (9x4 grid) bound to the player's inventory.
class InventoryGUIFactory : public GUIFactory {
public:
    struct Config {
        int columns = 9;           // Slots per row
        int rows = 4;              // Number of rows
        float slotSize = 32.0f;    // Size of each slot in pixels
        float slotSpacing = 4.0f;  // Spacing between slots
        float padding = 16.0f;     // Panel edge padding
    };

    static constexpr const char* PANEL_ID = "inventory";

    // GUIFactory interface
    const char* GetPanelId() const override { return PANEL_ID; }
    entt::entity Create(entt::registry& registry, entt::entity owner) override;
    void Destroy(entt::registry& registry, entt::entity panel) override;

    /// Create with custom configuration
    entt::entity Create(
        entt::registry& registry,
        entt::entity inventoryOwner,
        const Config& config
    );

private:
    Config currentConfig;
};

} // namespace ecs::factories::gui
