#include <ASCIICraft/ecs/systems/ViewBobbingSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/ViewBobbing.hpp>
#include <ASCIICraft/ecs/ViewBobbingMath.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <algorithm>
#include <cmath>

namespace ecs::systems {

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
        const bool flyingActive = flying && flying->enabled;
        const bool airborne = !ground.onGround;
        const bool inAir = airborne || flyingActive;

        const float horizSpeed = glm::length(glm::vec2(vel.linear.x, vel.linear.z));

        // Walk phase uses smoothed speed (blocks/s) so collision snaps on landing don't jerk sin/cos.
        bob.distanceWalkedModified += horizSpeed * dt * components::ViewBobbing::WALK_DISTANCE_SCALE;

        // Step counter keeps 3D position delta to stay aligned with footstep logic.
        const glm::vec3 physicsDelta = transform.position - transform.previousPosition;
        bob.distanceWalkedOnStepModified += glm::length(physicsDelta) * components::ViewBobbing::WALK_DISTANCE_SCALE;

        float targetCameraYaw = 0.0f;
        if (!inAir) {
            targetCameraYaw = (horizSpeed / components::PlayerController::WALK_SPEED)
                * components::ViewBobbing::MAX_CAMERA_YAW;
            targetCameraYaw = std::min(targetCameraYaw, components::ViewBobbing::MAX_CAMERA_YAW);
        }

        float targetCameraPitch = 0.0f;
        if (airborne && !flyingActive) {
            targetCameraPitch = std::atan(
                -vel.linear.y * components::ViewBobbing::VERTICAL_MOTION_FACTOR
            ) * components::ViewBobbing::PITCH_MOTION_SCALE;
        }

        bob.cameraYaw += (targetCameraYaw - bob.cameraYaw) * components::ViewBobbing::CAMERA_YAW_LERP;
        bob.cameraPitch += (targetCameraPitch - bob.cameraPitch) * components::ViewBobbing::CAMERA_PITCH_LERP;

        if (!bob.enabled
            || (std::abs(bob.cameraYaw) <= 1e-4f && std::abs(bob.cameraPitch) <= 1e-4f)) {
            continue;
        }

        const float renderAlpha = viewbobbing::ComputeRenderAlpha(transform);
        const float walkDelta = bob.distanceWalkedModified - bob.prevDistanceWalkedModified;
        const float walkPhase = -(bob.distanceWalkedModified + walkDelta * renderAlpha);

        const float interpCameraYaw = bob.prevCameraYaw
            + (bob.cameraYaw - bob.prevCameraYaw) * renderAlpha;
        const float interpCameraPitch = bob.prevCameraPitch
            + (bob.cameraPitch - bob.prevCameraPitch) * renderAlpha;

        const glm::mat4 bobMatrix = viewbobbing::BuildMinecraftBobMatrix(
            walkPhase,
            interpCameraYaw,
            interpCameraPitch,
            bob.intensity
        );
        cam.camera.view = bobMatrix * cam.camera.view;
    }
}

} // namespace ecs::systems
