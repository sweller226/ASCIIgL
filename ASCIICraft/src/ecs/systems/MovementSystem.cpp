#include <ASCIICraft/ecs/systems/MovementSystem.hpp>

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/Jump.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/util/Util.hpp>

namespace ecs::systems {

MovementSystem::MovementSystem(entt::registry& registry, ASCIICraft::IInputSource& input, EventBus& eventBus)
    : m_registry(registry)
    , m_input(input)
    , m_eventBus(eventBus)
{}

void MovementSystem::Update() {
    ProcessMovementInput();
}

void MovementSystem::ProcessMovementInput() {
    // --- Player entity via PlayerTag ---
    entt::entity p_ent = components::GetPlayerEntity(m_registry);

    if (p_ent == entt::null) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessMovementInput: Player entity not found.");
        return;
    }

    if (p_ent == entt::null || !m_registry.valid(p_ent)) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessMovementInput: player entity is null or invalid.");
        return;
    }

    // --- Required components check ---

    if (!m_registry.all_of<
        components::PlayerCamera,
        components::PlayerController,
        components::Jump,
        components::Velocity,
        components::GroundPhysics,
        components::FlyingPhysics,
        components::Gravity,
        components::Transform,
        components::PlayerMode
    >(p_ent)) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessMovementInput: missing one or more required components on player.");
        return;
    }

    // --- Safe component retrieval ---
    auto* cam      = m_registry.try_get<components::PlayerCamera>(p_ent);
    auto* ctrl     = m_registry.try_get<components::PlayerController>(p_ent);
    auto* jump     = m_registry.try_get<components::Jump>(p_ent);
    auto* vel      = m_registry.try_get<components::Velocity>(p_ent);
    auto* groundPhy= m_registry.try_get<components::GroundPhysics>(p_ent);
    auto* flyingPhy= m_registry.try_get<components::FlyingPhysics>(p_ent);
    auto* grav     = m_registry.try_get<components::Gravity>(p_ent);
    auto* tx       = m_registry.try_get<components::Transform>(p_ent);
    auto* pmode    = m_registry.try_get<components::PlayerMode>(p_ent);

    if (!cam || !ctrl || !jump || !vel || !groundPhy || !flyingPhy || !grav || !tx || !pmode) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessMovementInput: try_get returned NULL for one or more components.");
        return;
    }

    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    const float currentTime = NowSeconds();

    // --- input -> desired horizontal direction ---
    glm::vec3 forward = cam->camera.getCamFrontNoY();
    glm::vec3 right   = cam->camera.getCamRightNoY();
    glm::vec3 moveDir(0.0f);

    if (m_input.IsActionHeld("move_forward"))  moveDir += forward;
    if (m_input.IsActionHeld("move_backward")) moveDir -= forward;
    if (m_input.IsActionHeld("move_left"))     moveDir -= right;
    if (m_input.IsActionHeld("move_right"))   moveDir += right;

    glm::vec2 moveXZ(moveDir.x, moveDir.z);
    float moveLen = glm::length(moveXZ);
    if (moveLen > 1e-6f) moveXZ /= moveLen;

    // --- movement state ---
    if (pmode->gamemode == GameMode::Spectator) {
        ctrl->movementState = MovementState::Flying;
    } else if (m_input.IsActionHeld("sprint") && moveLen > 0.01f && !(ctrl->movementState == MovementState::Sneaking)) {
        ctrl->movementState = MovementState::Running;
    } else if (m_input.IsActionHeld("sneak") && !ctrl->isFlying() && (pmode->gamemode != GameMode::Spectator)) {
        ctrl->movementState = MovementState::Sneaking;
    } else {
        ctrl->movementState = MovementState::Walking;
    }

    float targetSpeed = 0.0f;

    switch (ctrl->movementState) {
        case MovementState::Walking:
            targetSpeed = ctrl->WALK_SPEED;
            break;

        case MovementState::Sneaking:
            targetSpeed = ctrl->SNEAK_SPEED;
            break;

        case MovementState::Running:
            targetSpeed = ctrl->RUN_SPEED;
            break;

        case MovementState::Flying:
            targetSpeed = ctrl->FLY_SPEED;
            break;
    }

    glm::vec3 desiredHoriz(moveXZ.x * targetSpeed, 0.0f, moveXZ.y * targetSpeed);

    // --- jump cooldown ---
    jump->jumpCooldown = std::max(0.0f, jump->jumpCooldown - dt);

    // --- jump buffer (from discrete JumpPressedEvent; InputSystem emits when jump is pressed and not blocking) ---
    jump->jumpBufferTimer = std::max(0.0f, jump->jumpBufferTimer - dt);
    for ([[maybe_unused]] const auto& e : m_eventBus.view<events::JumpPressedEvent>()) {
        jump->jumpBufferTimer = jump->JUMP_BUFFER_MAX;
        break;
    }

    // --- ground / coyote time ---
    if (groundPhy->onGround) {
        groundPhy->lastOnGround = currentTime;
    }

    // --- horizontal acceleration ---
    glm::vec3 currentHoriz(vel->linear.x, 0.0f, vel->linear.z);
    float accel = groundPhy->onGround ? ctrl->GROUND_ACCEL : ctrl->AIR_ACCEL;
    if (flyingPhy->enabled) { accel = ctrl->GROUND_ACCEL; }

    glm::vec3 delta = desiredHoriz - currentHoriz;
    float maxStep = accel * dt;
    float deltaLen = glm::length(delta);
    if (deltaLen > maxStep && deltaLen > 0.0f) delta *= (maxStep / deltaLen);
    currentHoriz += delta;

    vel->linear.x = currentHoriz.x;
    vel->linear.z = currentHoriz.z;

    float horizSpeed2 = currentHoriz.x * currentHoriz.x + currentHoriz.z * currentHoriz.z;
    if (targetSpeed > 0.0f && horizSpeed2 > targetSpeed * targetSpeed) {
        glm::vec2 clamped = glm::normalize(glm::vec2(currentHoriz.x, currentHoriz.z)) * targetSpeed;
        vel->linear.x = clamped.x;
        vel->linear.z = clamped.y;
    }

    // --- jump logic ---
    bool coyoteAllowed = (currentTime - groundPhy->lastOnGround) <= jump->COYOTE_TIME_MAX;
    bool canJumpNow = (groundPhy->onGround || coyoteAllowed) &&
                      jump->jumpCooldown <= 0.0f &&
                      !ctrl->isFlying();
    bool bufferedJump = jump->jumpBufferTimer > 0.0f;

    if (bufferedJump && canJumpNow) {
        float g = std::abs(grav->acceleration.y);
        float jumpVelocity = std::sqrt(2.0f * g * jump->jumpHeight);
        vel->linear.y = jumpVelocity;

        if (ctrl->isRunning()) {
            glm::vec3 fwd = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
            vel->linear.x += fwd.x * targetSpeed;
            vel->linear.z += fwd.z * targetSpeed;
        }

        groundPhy->onGround = false;
        groundPhy->lastOnGround = 0.0f;  // Reset coyote time to prevent multi-jump
        jump->jumpCooldown = jump->jumpCooldownMax;
        jump->jumpBufferTimer = 0.0f;
    }

    // --- flying / spectator vertical control ---
    if (ctrl->isFlying() || pmode->gamemode == GameMode::Spectator) {
        if (m_input.IsActionHeld("jump"))       vel->linear.y = targetSpeed;
        else if (m_input.IsActionHeld("sneak")) vel->linear.y = -targetSpeed;
        else                                  vel->linear.y = 0.0f;
    }

}


}