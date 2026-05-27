// ecs/systems/LifetimeSystem.hpp
#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

namespace ecs::systems {

class LifetimeSystem : public ISystem {
public:
    LifetimeSystem(entt::registry& registry);

    void Update() override;

private:
    entt::registry& m_registry;
};

}