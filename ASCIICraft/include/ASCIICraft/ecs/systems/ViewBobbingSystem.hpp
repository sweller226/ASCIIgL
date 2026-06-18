#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

namespace ecs::systems {

class ViewBobbingSystem : public ISystem {
public:
    explicit ViewBobbingSystem(entt::registry& registry);

    void Update() override;

private:
    entt::registry& m_registry;
};

} // namespace ecs::systems
