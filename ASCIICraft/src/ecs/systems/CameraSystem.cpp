#include <ASCIICraft/ecs/systems/CameraSystem.hpp>

#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/ecs/managers/PlayerManager.hpp>

namespace ecs::systems {

void CameraSystem::Update() {
    const auto &input = ASCIIgL::InputManager::GetInst();

    if (!m_registry.ctx().contains<managers::PlayerManager>()) return;
    auto &pm = m_registry.ctx().get<managers::PlayerManager>();
    auto p_ent = pm.getPlayerEnt();
    if (p_ent == entt::null || !m_registry.valid(p_ent)) return;

    // required components
    if (!m_registry.all_of<
        components::PlayerCamera
    >(p_ent)) return;

    auto &cam = m_registry.get<components::PlayerCamera>(p_ent);
    auto &ctrl = m_registry.get<components::PlayerController>(p_ent);
    
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    ProcessCameraInput(input, cam, dt);
    LerpFOV(input, cam, ctrl, dt);
}

void CameraSystem::ProcessCameraInput(const ASCIIgL::InputManager &input, components::PlayerCamera &cam, const float dt) {
    // Use keyboard controls for camera movement (arrow keys only)
    
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;

    // Arrow keys for camera rotation
    if (input.IsActionHeld("camera_left")) {
        yawDelta -= input.GetMouseSensitivity() * dt;
    }
    if (input.IsActionHeld("camera_right")) {
        yawDelta += input.GetMouseSensitivity() * dt;
    }
    if (input.IsActionHeld("camera_up")) {
        pitchDelta += input.GetMouseSensitivity() * dt;
    }
    if (input.IsActionHeld("camera_down")) {
        pitchDelta -= input.GetMouseSensitivity() * dt;
    }
    
    // Update camera direction
    cam.camera->setCamDir(cam.camera->GetYaw() + yawDelta, cam.camera->GetPitch() + pitchDelta);
}

void CameraSystem::LerpFOV(const ASCIIgL::InputManager &input, components::PlayerCamera &cam, components::PlayerController ctrl, const float dt) {
    // Smoothly adjust FOV when sprinting (slight increase for speed effect)
    float targetFOV = cam.FOV;
    if (ctrl.isRunning()) {
        targetFOV = cam.FOV + 10.0f;  // +10 degrees when sprinting
    }
    
    // Smoothly interpolate FOV towards target (smooth transition)
    float currentFOV = cam.camera->GetFov();
    float fovTransitionSpeed = 8.0f; // How fast FOV changes (higher = faster)
    
    // Lerp FOV: currentFOV + (targetFOV - currentFOV) * speed * deltaTime
    float newFOV = currentFOV + (targetFOV - currentFOV) * fovTransitionSpeed * dt;
    
    // Update camera FOV
    cam.camera->SetFov(newFOV);
}

}