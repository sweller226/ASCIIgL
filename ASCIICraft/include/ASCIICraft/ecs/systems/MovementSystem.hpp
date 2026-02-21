#pragma once

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

class EventBus;

namespace ecs::systems {

class MovementSystem : public ISystem {
public:
    MovementSystem(entt::registry& registry, ASCIICraft::IInputSource& input, EventBus& eventBus);

    void Update() override;

private:
    entt::registry& m_registry;
    ASCIICraft::IInputSource& m_input;
    EventBus& m_eventBus;

    void ProcessMovementInput();
};

}