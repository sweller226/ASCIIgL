#pragma once

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

namespace ASCIIgL { class EventBus; }

namespace ecs::systems {

class MovementSystem : public ISystem {
public:
    MovementSystem(entt::registry& registry, IInputSource& input, ASCIIgL::EventBus& eventBus);

    void Update() override;

private:
    entt::registry& m_registry;
    IInputSource& m_input;
    ASCIIgL::EventBus& m_eventBus;

    void ProcessMovementInput();
    void ProcessSwitchGameModeEvents();
};

}