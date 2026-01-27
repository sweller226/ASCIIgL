#pragma once

#include <ASCIICraft/events/EventBus.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class PlacingSystem {
public:
    explicit PlacingSystem(entt::registry &registry, EventBus& eventBus) noexcept;
    PlacingSystem(const PlacingSystem&) = delete;
    PlacingSystem& operator=(const PlacingSystem&) = delete;

    void Update();

private:
    entt::registry &m_registry;
    EventBus &eventBus;

    void PlayerPlace(); // temp simple insta break
};

}

