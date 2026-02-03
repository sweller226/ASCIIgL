#pragma once

#include <ASCIICraft/ecs/factories/gui/GUIFactory.hpp>

namespace ecs::factories::gui {

/// Factory for creating the hotbar GUI.
/// Creates a horizontal bar with 9 slots at the bottom of the screen.
/// The hotbar is always visible (not a toggleable panel).
class HotbarGUIFactory : public GUIFactory {
public:
    struct Config {
        int slots = 9;             // Number of hotbar slots
        float slotSize = 32.0f;    // Size of each slot in pixels
        float slotSpacing = 4.0f;  // Spacing between slots
        float bottomMargin = 16.0f;// Distance from bottom of screen
    };

    static constexpr const char* PANEL_ID = "hotbar";

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
