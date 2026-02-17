#pragma once

#include <entt/entt.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>
#include <ASCIICraft/input/IGameInputSource.hpp>

namespace ecs::systems {

class CameraSystem : public ISystem {
public:
    CameraSystem(entt::registry& registry, ASCIICraft::IGameInputSource& input);

    void Update() override;

private:
    entt::registry& m_registry;
    ASCIICraft::IGameInputSource& m_input;

    void ProcessCameraInput(components::PlayerCamera& cam, float dt);
    void LerpFOV(components::PlayerCamera& cam, components::PlayerController& ctrl, float dt);
};

}