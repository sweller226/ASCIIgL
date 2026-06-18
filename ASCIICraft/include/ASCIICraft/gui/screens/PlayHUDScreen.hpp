#pragma once

#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/gui/GUISurface.hpp>
#include <ASCIICraft/gui/GUISurfaceLibrary.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace ASCIIgL { class EventBus; }

namespace gui {

/// Non-blocking play HUD: hotbar (9 slots) at bottom center.
class PlayHUDScreen : public GUIScreen {
public:
    PlayHUDScreen(entt::registry& registry,
                  ASCIIgL::EventBus& eventBus,
                  entt::entity playerEntity,
                  GUISurfaceLibrary& surfaceLibrary);

    bool OnClick(glm::vec2 position, int button) override;
    void Layout(glm::vec2 screenSize, const glm::vec2* parentTopLeft = nullptr) override;
    void Draw(GUIRenderer& renderer) const override;

private:
    entt::registry* m_registry = nullptr;
    ASCIIgL::EventBus* m_eventBus = nullptr;
    entt::entity m_playerEntity = entt::null;
    GUISurface m_hotbarBgSurface{};
    GUISurface m_hotbarSelectionSurface{};
    glm::vec2 m_hotbarTopLeft{0.0f, 0.0f};
};

} // namespace gui
