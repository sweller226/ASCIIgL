#pragma once

#include <entt/entt.hpp>

#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>

namespace ecs::systems {

class CameraSystem {
public:
    explicit CameraSystem(entt::registry &registry) noexcept;
    CameraSystem(const CameraSystem&) = delete;
    CameraSystem& operator=(const CameraSystem&) = delete;

    void Update();    

private:
    entt::registry &m_registry;

    void ProcessCameraInput(const ASCIIgL::InputManager &input, components::PlayerCamera &cam, const float dt);
    void LerpFOV(const ASCIIgL::InputManager &input, components::PlayerCamera &cam, components::PlayerController &ctrl, const float dt);
};

}