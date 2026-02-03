#pragma once

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class PlacingSystem : public ISystem {
public:
    explicit PlacingSystem(entt::registry &registry, EventBus& eventBus);

    void Update() override;

private:
    entt::registry &m_registry;
    EventBus &eventBus;

    void PlayerPlace(); // temp simple insta break
};

}

