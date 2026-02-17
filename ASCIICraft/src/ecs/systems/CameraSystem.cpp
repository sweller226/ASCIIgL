#include <ASCIICraft/ecs/systems/CameraSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>

namespace ecs::systems {

CameraSystem::CameraSystem(entt::registry& registry, ASCIICraft::IGameInputSource& input)
    : m_registry(registry)
    , m_input(input)
{}

void CameraSystem::Update() {
    entt::entity p_ent = components::GetPlayerEntity(m_registry);
    if (p_ent == entt::null || !m_registry.valid(p_ent)) return;

    if (!m_registry.all_of<components::PlayerCamera, components::PlayerController>(p_ent)) return;

    auto& cam = m_registry.get<components::PlayerCamera>(p_ent);
    auto& ctrl = m_registry.get<components::PlayerController>(p_ent);
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    ProcessCameraInput(cam, dt);
    LerpFOV(cam, ctrl, dt);

    auto& head = m_registry.get<components::Head>(p_ent);
    head.lookDir = cam.camera.getCamFront();
}

void CameraSystem::ProcessCameraInput(components::PlayerCamera& cam, float dt) {
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;

    if (m_input.IsActionHeld("camera_left")) {
        yawDelta -= m_input.GetMouseSensitivity() * dt;
    }
    if (m_input.IsActionHeld("camera_right")) {
        yawDelta += m_input.GetMouseSensitivity() * dt;
    }
    if (m_input.IsActionHeld("camera_up")) {
        pitchDelta += m_input.GetMouseSensitivity() * 0.8f * dt;
    }
    if (m_input.IsActionHeld("camera_down")) {
        pitchDelta -= m_input.GetMouseSensitivity() * 0.8f * dt;
    }

    cam.camera.setCamDir(cam.camera.GetYaw() + yawDelta, cam.camera.GetPitch() + pitchDelta);
}

void CameraSystem::LerpFOV(components::PlayerCamera& cam, components::PlayerController& ctrl, float dt) {
    // Smoothly adjust FOV when sprinting (slight increase for speed effect)
    float targetFOV = cam.FOV;
    if (ctrl.isRunning()) {
        targetFOV = cam.FOV + 10.0f;  // +10 degrees when sprinting
    }
    
    // Smoothly interpolate FOV towards target (smooth transition)
    float currentFOV = cam.camera.GetFov();
    float fovTransitionSpeed = 8.0f; // How fast FOV changes (higher = faster)
    
    // Lerp FOV: currentFOV + (targetFOV - currentFOV) * speed * deltaTime
    float newFOV = currentFOV + (targetFOV - currentFOV) * fovTransitionSpeed * dt;
    
    // Update camera FOV
    cam.camera.SetFov(newFOV);
}

}