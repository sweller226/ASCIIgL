#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

namespace ecs::systems {

class HotbarSystem : public ISystem {
public:
    HotbarSystem(entt::registry& registry, IInputSource& input);
    void Update() override;

private:
    entt::registry& registry;
    IInputSource& m_input;
};

} // namespace ecs::systems
