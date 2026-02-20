#pragma once

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class MiningSystem : public ISystem {
public:
    MiningSystem(entt::registry& registry, EventBus& eventBus);

    void Update() override;

private:
    entt::registry& m_registry;
    EventBus& m_eventBus;

    void CreativeBreakEvents();
};

}

