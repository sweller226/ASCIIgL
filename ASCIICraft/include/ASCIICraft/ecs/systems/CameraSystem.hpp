#pragma once

#include <entt/entt.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/input/IInputSource.hpp>

namespace ecs::systems {

class CameraSystem : public ISystem {
public:
    CameraSystem(entt::registry& registry, IInputSource& input);

    void Update() override;

private:
    entt::registry& m_registry;
    IInputSource& m_input;

    void ProcessCameraInput(components::PlayerCamera& cam, float dt);
    void LerpFOV(components::PlayerCamera& cam, components::PlayerController& ctrl, float dt);
};

}