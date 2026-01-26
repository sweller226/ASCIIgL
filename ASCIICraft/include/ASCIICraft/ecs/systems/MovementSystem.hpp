#pragma once

#include <entt/entt.hpp>

#include <glm/glm.hpp>

namespace ecs::systems {

class MovementSystem {
public:
    explicit MovementSystem(entt::registry &registry) noexcept;
    MovementSystem(const MovementSystem&) = delete;
    MovementSystem& operator=(const MovementSystem&) = delete;

    void Update();      

private:
    entt::registry &m_registry;

    void ProcessMovementInput();
    void ApplyMoveDir(const glm::vec3& direction);
};

}