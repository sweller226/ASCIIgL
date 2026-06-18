#pragma once

#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/gui/GUISurfaceLibrary.hpp>
#include <ASCIICraft/gui/GUISurface.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace ASCIIgL { class Texture; }
namespace ASCIIgL { class EventBus; }

namespace gui {

/// Blocking inventory screen: 9×4 slot grid. E to toggle (handled by GUIManager).
class InventoryScreen : public GUIScreen {
public:
    /// inventoryTexture is the single inventory PNG (Minecraft-style).
    InventoryScreen(entt::registry& registry, ASCIIgL::EventBus& eventBus, entt::entity playerEntity,
                   GUISurfaceLibrary& meshLibrary);

    void Draw(GUIRenderer& renderer) const override;
    bool OnClick(glm::vec2 position, int button) override;
    bool TryGetInitialCursorPosition(glm::vec2 screenSize, glm::vec2& out) const override;

private:
    entt::registry* m_registry = nullptr;
    ASCIIgL::EventBus* m_eventBus = nullptr;
    GUISurface m_inventoryPanelSurface{};
};

} // namespace gui
