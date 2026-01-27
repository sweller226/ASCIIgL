#pragma once

#include <ASCIICraft/events/EventBus.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class MiningSystem {
public:
    explicit MiningSystem(entt::registry &registry, EventBus& eventBus) noexcept;
    MiningSystem(const MiningSystem&) = delete;
    MiningSystem& operator=(const MiningSystem&) = delete;

    void Update();

private:
    entt::registry &m_registry;
    EventBus &eventBus;

    void CreativeBreakEvents(); // temp simple insta break
};

}

