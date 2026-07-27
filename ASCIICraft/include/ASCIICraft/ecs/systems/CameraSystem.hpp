#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>
#include <string>

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
    glm::vec2 m_smoothedLook{0.0f, 0.0f};

    void ProcessCameraInput(components::PlayerCamera& cam, float dt);
    void LerpFOV(components::PlayerCamera& cam, components::PlayerController& ctrl, float dt);
    void LerpPlayerHeight(components::PlayerCamera& cam, components::PlayerController& ctrl, float dt);

    std::string m_lastCardinal;
};

}
