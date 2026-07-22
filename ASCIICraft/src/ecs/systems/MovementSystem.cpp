#include <ASCIICraft/ecs/systems/MovementSystem.hpp>

#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <entt/entt.hpp>

#include <algorithm>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/PlayerController.hpp>
#include <ASCIICraft/ecs/components/Jump.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>

namespace ecs::systems {

namespace {

void DisableCreativeFlight(
    components::PlayerController& ctrl,
    components::FlyingPhysics& flying,
    components::Velocity& vel
) {
    flying.enabled = false;
    ctrl.movementState = MovementState::Walking;
    ctrl.flyToggleTimer = 0.0f;
    ctrl.flyLandGrace = 0.0f;
    vel.linear.y = 0.0f;
}

void EnableCreativeFlight(
    components::PlayerController& ctrl,
    components::FlyingPhysics& flying,
    components::GroundPhysics& ground,
    components::Velocity& vel,
    components::Jump& jump
) {
    flying.enabled = true;
    ctrl.movementState = MovementState::Flying;
    ctrl.flyToggleTimer = 0.0f;
    ctrl.flyLandGrace = components::PlayerController::FLY_LAND_GRACE;
    ground.onGround = false;
    vel.linear.y = std::max(vel.linear.y, 0.5f);
    jump.bufferTime = 0.0f;
}

} // namespace

MovementSystem::MovementSystem(entt::registry& registry, IInputSource& input, ASCIIgL::EventBus& eventBus)
    : m_registry(registry)
    , m_input(input)
    , m_eventBus(eventBus)
{}

void MovementSystem::Update() {
    ProcessSwitchGameModeEvents();
    ProcessMovementInput();
}

void MovementSystem::ProcessSwitchGameModeEvents() {
    // Toggle once per frame if any SwitchGameModeEvent was emitted.
    const auto& events = m_eventBus.view<events::SwitchGameModeEvent>();
    if (events.empty()) return;

    entt::entity p_ent = components::GetPlayerEntity(m_registry);
    if (p_ent == entt::null || !m_registry.valid(p_ent)) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessSwitchGameModeEvents: Player entity not found or invalid.");
        return;
    }

    if (!m_registry.all_of<
        components::PlayerController,
        components::FlyingPhysics,
        components::Gravity,
        components::Collider,
        components::PlayerMode,
        components::Velocity
    >(p_ent)) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessSwitchGameModeEvents: missing components on player.");
        return;
    }

    auto* ctrl     = m_registry.try_get<components::PlayerController>(p_ent);
    auto* flying   = m_registry.try_get<components::FlyingPhysics>(p_ent);
    auto* col      = m_registry.try_get<components::Collider>(p_ent);
    auto* pmode    = m_registry.try_get<components::PlayerMode>(p_ent);
    auto* vel      = m_registry.try_get<components::Velocity>(p_ent);

    if (!ctrl || !flying || !col || !pmode || !vel) {
        ASCIIgL::Logger::Error("MovementSystem::ProcessSwitchGameModeEvents: try_get returned NULL for one or more components.");
        return;
    }

    // Cycle Survival -> Creative -> Spectator, mirroring PlayerFactory defaults.
    switch (pmode->gamemode) {
        case GameMode::Survival:
            pmode->gamemode = GameMode::Creative;
            ctrl->movementState = MovementState::Walking;
            flying->enabled = false;
            col->disabled = false;
            ctrl->flyToggleTimer = 0.0f;
            ctrl->flyLandGrace = 0.0f;
            ASCIIgL::Logger::Info("Game mode: Creative");
            break;
        case GameMode::Creative:
            // Leaving Creative clears any in-mode flight before Spectator takes over.
            pmode->gamemode = GameMode::Spectator;
            ctrl->movementState = MovementState::Flying;
            flying->enabled = true;
            col->disabled = true;
            ctrl->flyToggleTimer = 0.0f;
            ctrl->flyLandGrace = 0.0f;
            ASCIIgL::Logger::Info("Game mode: Spectator");
            break;
        case GameMode::Spectator:
            pmode->gamemode = GameMode::Survival;
            ctrl->movementState = MovementState::Walking;
            flying->enabled = false;
            col->disabled = false;
            ctrl->flyToggleTimer = 0.0f;
            ctrl->flyLandGrace = 0.0f;
            vel->linear.y = 0.0f;
            ASCIIgL::Logger::Info("Game mode: Survival");
            break;
    }
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

    ctrl->flyToggleTimer = std::max(0.0f, ctrl->flyToggleTimer - dt);
    ctrl->flyLandGrace = std::max(0.0f, ctrl->flyLandGrace - dt);

    // Landing ends creative flight (vanilla). Grace avoids instant cancel right after toggle-on.
    if (pmode->gamemode == GameMode::Creative
        && flyingPhy->enabled
        && groundPhy->onGround
        && ctrl->flyLandGrace <= 0.0f) {
        DisableCreativeFlight(*ctrl, *flyingPhy, *vel);
    }

    // Creative: double-tap Space toggles flight.
    bool toggledCreativeFlight = false;
    if (pmode->gamemode == GameMode::Creative && m_input.IsActionPressed("jump")) {
        if (ctrl->flyToggleTimer > 0.0f) {
            if (flyingPhy->enabled) {
                DisableCreativeFlight(*ctrl, *flyingPhy, *vel);
            } else {
                EnableCreativeFlight(*ctrl, *flyingPhy, *groundPhy, *vel, *jump);
            }
            toggledCreativeFlight = true;
        } else {
            ctrl->flyToggleTimer = components::PlayerController::FLY_TOGGLE_WINDOW;
        }
    }

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
    const bool sprintHeld = m_input.IsActionHeld("sprint") && moveLen > 0.01f;
    if (pmode->gamemode == GameMode::Spectator) {
        ctrl->movementState = MovementState::Flying;
    } else if (flyingPhy->enabled) {
        // Creative flight: keep Flying; sprint only affects speed below.
        ctrl->movementState = MovementState::Flying;
    } else if (sprintHeld && !(ctrl->movementState == MovementState::Sneaking)) {
        ctrl->movementState = MovementState::Running;
    } else if (m_input.IsActionHeld("sneak") && !ctrl->isFlying() && (pmode->gamemode != GameMode::Spectator)) {
        ctrl->movementState = MovementState::Sneaking;
    } else {
        ctrl->movementState = MovementState::Walking;
    }

    // Sprint FOV / speed cue: ground run, or Ctrl while creative-flying with move input.
    ctrl->sprinting = (ctrl->movementState == MovementState::Running)
        || (pmode->gamemode == GameMode::Creative && flyingPhy->enabled && sprintHeld);

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
            if (pmode->gamemode == GameMode::Spectator) {
                targetSpeed = ctrl->SPECTATOR_FLY_SPEED;
            } else if (sprintHeld) {
                targetSpeed = ctrl->CREATIVE_FLY_SPRINT_SPEED;
            } else {
                targetSpeed = ctrl->CREATIVE_FLY_SPEED;
            }
            break;
    }

    glm::vec3 desiredHoriz(moveXZ.x * targetSpeed, 0.0f, moveXZ.y * targetSpeed);

    // --- jump timers ---
    jump->jumpCooldown = std::max(0.0f, jump->jumpCooldown - dt);
    jump->bufferTime = std::max(0.0f, jump->bufferTime - dt);

    if (m_input.IsActionPressed("jump") && !toggledCreativeFlight) {
        jump->bufferTime = jump->JUMP_BUFFER_MAX;
    }

    if (!groundPhy->onGround) {
        jump->coyoteTime = std::max(0.0f, jump->coyoteTime - dt);
    } else {
        jump->coyoteTime = jump->COYOTE_TIME_MAX;
    }

    // --- horizontal acceleration ---
    // Flying: snap to desired velocity (same as vertical). Walk accel at fly speeds
    // takes too long to start/stop and feels like ice.
    if (flyingPhy->enabled) {
        vel->linear.x = desiredHoriz.x;
        vel->linear.z = desiredHoriz.z;
    } else {
        glm::vec3 currentHoriz(vel->linear.x, 0.0f, vel->linear.z);
        float accel = groundPhy->onGround ? ctrl->GROUND_ACCEL : ctrl->AIR_ACCEL;

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
    }

    // --- jump logic ---
    const bool jumpHeld = m_input.IsActionHeld("jump");
    const bool wantsJump = jump->bufferTime > 0.0f
        || (jumpHeld && groundPhy->onGround);
    const bool canJump = (groundPhy->onGround || jump->coyoteTime > 0.0f)
        && jump->jumpCooldown <= 0.0f
        && !ctrl->isFlying()
        && !toggledCreativeFlight;

    if (wantsJump && canJump) {
        jump->RefreshJumpVelocity(grav->acceleration.y);
        vel->linear.y = jump->jumpVelocity;

        if (ctrl->isRunning()) {
            glm::vec3 fwd = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
            vel->linear.x += fwd.x * targetSpeed;
            vel->linear.z += fwd.z * targetSpeed;
        }

        groundPhy->onGround = false;
        jump->coyoteTime = 0.0f;
        jump->jumpCooldown = jump->JUMP_COOLDOWN_MAX;
        jump->bufferTime = 0.0f;
    }

    // --- flying / spectator vertical control ---
    if (ctrl->isFlying() || pmode->gamemode == GameMode::Spectator) {
        if (m_input.IsActionHeld("jump"))       vel->linear.y = targetSpeed;
        else if (m_input.IsActionHeld("sneak")) vel->linear.y = -targetSpeed;
        else                                  vel->linear.y = 0.0f;
    }

}


}
