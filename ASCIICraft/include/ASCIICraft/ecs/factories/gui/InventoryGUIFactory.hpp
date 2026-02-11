#pragma once

#include <ASCIICraft/ecs/factories/gui/GUIFactory.hpp>

namespace ecs::factories::gui {

/// Factory for creating the inventory panel GUI.
/// Creates a panel with 36 slots (9x4 grid) bound to the player's inventory.
class InventoryGUIFactory : public GUIFactory {
public:
    static constexpr const char* PANEL_ID = "inventory";

    // GUIFactory interface
    const char* GetPanelId() const override { return PANEL_ID; }
    entt::entity Create(entt::registry& registry, entt::entity owner) override;
    void Destroy(entt::registry& registry, entt::entity panel) override;
};

} // namespace ecs::factories::gui
