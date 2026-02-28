#pragma once

#include <ASCIICraft/gui/Screen.hpp>
#include <entt/entt.hpp>
#include <memory>

class EventBus;

namespace gui {

/// Non-blocking play HUD: hotbar (9 slots) at bottom center.
class PlayHUDScreen : public Screen {
public:
    PlayHUDScreen(entt::registry& registry, EventBus& eventBus, entt::entity playerEntity);

    bool OnClick(glm::vec2 position, int button) override;

private:
    entt::registry* m_registry = nullptr;
    EventBus* m_eventBus = nullptr;
};

} // namespace gui
