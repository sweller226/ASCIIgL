#include <ASCIICraft/ecs/systems/ViewBobbingSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/ViewBobbing.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace ecs::systems {

namespace {

float ComputeRenderAlpha(const components::Transform& transform) {
    const glm::vec3 denom = transform.position - transform.previousPosition;
    if (glm::dot(denom, denom) < 1e-10f) {
        return 1.0f;
    }

    const glm::vec3 numer = transform.renderPosition - transform.previousPosition;
    return glm::clamp(glm::dot(numer, denom) / glm::dot(denom, denom), 0.0f, 1.0f);
}

glm::mat4 BuildMinecraftBobMatrix(
    float walkPhase,
    float cameraYaw,
    float cameraPitch,
    float intensity
) {
    cameraYaw *= intensity;
    cameraPitch *= intensity;

    const float phase = walkPhase * glm::pi<float>();

    const glm::vec3 localTranslate(
        std::sin(phase) * cameraYaw * 0.5f,
        -std::abs(std::cos(phase) * cameraYaw),
        0.0f
    );
    const float rollDegrees = std::sin(phase) * cameraYaw * 3.0f;
    const float pitchBobDegrees = std::abs(std::cos(phase - 0.2f) * cameraYaw) * 5.0f;

    glm::mat4 bob(1.0f);
    bob = glm::translate(bob, localTranslate);
    bob = glm::rotate(bob, glm::radians(rollDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    bob = glm::rotate(bob, glm::radians(pitchBobDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    bob = glm::rotate(bob, glm::radians(cameraPitch), glm::vec3(1.0f, 0.0f, 0.0f));
    return bob;
}

} // namespace

ViewBobbingSystem::ViewBobbingSystem(entt::registry& registry)
    : m_registry(registry)
{}

void ViewBobbingSystem::Update() {
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    auto view = m_registry.view<
        components::ViewBobbing,
        components::PlayerCamera,
        components::Transform,
        components::Velocity,
        components::GroundPhysics
    >();

    for (auto [ent, bob, cam, transform, vel, ground] : view.each()) {
        const glm::vec3 baseEyePos = transform.renderPosition
            + glm::vec3(0.0f, cam.PLAYER_EYE_HEIGHT, 0.0f);
        cam.camera.setCamPos(baseEyePos);

        bob.prevDistanceWalkedModified = bob.distanceWalkedModified;
        bob.prevCameraYaw = bob.cameraYaw;
        bob.prevCameraPitch = bob.cameraPitch;

        const auto* flying = m_registry.try_get<components::FlyingPhysics>(ent);
        const bool inAir = !ground.onGround || (flying && flying->enabled);

        const float horizSpeed = glm::length(glm::vec2(vel.linear.x, vel.linear.z));

        // Walk phase uses smoothed speed (blocks/s) so collision snaps on landing don't jerk sin/cos.
        bob.distanceWalkedModified += horizSpeed * dt * components::ViewBobbing::WALK_DISTANCE_SCALE;

        // Step counter keeps 3D position delta to stay aligned with footstep logic.
        const glm::vec3 physicsDelta = transform.position - transform.previousPosition;
        bob.distanceWalkedOnStepModified += glm::length(physicsDelta) * components::ViewBobbing::WALK_DISTANCE_SCALE;

        if (inAir) {
            // Bob is not applied in air, so don't let pitch/yaw build up and dump on landing.
            bob.cameraYaw = 0.0f;
            bob.cameraPitch = 0.0f;
            bob.prevCameraYaw = 0.0f;
            bob.prevCameraPitch = 0.0f;
            continue;
        }

        float targetCameraYaw = (horizSpeed / components::PlayerController::WALK_SPEED)
            * components::ViewBobbing::MAX_CAMERA_YAW;
        targetCameraYaw = std::min(targetCameraYaw, components::ViewBobbing::MAX_CAMERA_YAW);

        // Pitch bob from vertical motion only applies in air in vanilla; we skip it entirely.
        constexpr float targetCameraPitch = 0.0f;

        bob.cameraYaw += (targetCameraYaw - bob.cameraYaw) * components::ViewBobbing::CAMERA_YAW_LERP;
        bob.cameraPitch += (targetCameraPitch - bob.cameraPitch) * components::ViewBobbing::CAMERA_PITCH_LERP;

        if (!bob.enabled || bob.cameraYaw <= 1e-4f) {
            continue;
        }

        const float renderAlpha = ComputeRenderAlpha(transform);
        const float walkDelta = bob.distanceWalkedModified - bob.prevDistanceWalkedModified;
        const float walkPhase = -(bob.distanceWalkedModified + walkDelta * renderAlpha);

        const float interpCameraYaw = bob.prevCameraYaw
            + (bob.cameraYaw - bob.prevCameraYaw) * renderAlpha;
        const float interpCameraPitch = bob.prevCameraPitch
            + (bob.cameraPitch - bob.prevCameraPitch) * renderAlpha;

        const glm::mat4 bobMatrix = BuildMinecraftBobMatrix(
            walkPhase,
            interpCameraYaw,
            interpCameraPitch,
            bob.intensity
        );
        cam.camera.view = bobMatrix * cam.camera.view;
    }
}

} // namespace ecs::systems
