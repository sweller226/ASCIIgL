#pragma once

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

namespace ecs::systems {

class MovementSystem : public ISystem {
public:
    explicit MovementSystem(entt::registry &registry) noexcept;
    MovementSystem(const MovementSystem&) = delete;
    MovementSystem& operator=(const MovementSystem&) = delete;

    void Update() override;      

private:
    entt::registry &m_registry;

    void ProcessMovementInput();
};

}