#pragma once

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class PlacingSystem : public ISystem {
public:
    PlacingSystem(entt::registry& registry, EventBus& eventBus);

    void Update() override;

private:
    entt::registry& m_registry;
    EventBus& m_eventBus;
    uint16_t m_selectedTypeId{0};
    uint32_t m_selectedStateId{0};
    bool m_selectionInitialized{false};

    void HandleDebugBlockSelection();
    void PlayerPlace();
};

}

