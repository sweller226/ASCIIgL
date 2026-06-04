#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

namespace ecs::systems {

class HotbarSystem : public ISystem {
public:
    explicit HotbarSystem(entt::registry& registry);
    void Update() override;

private:
    entt::registry& registry;
};

} // namespace ecs::systems
