#include <ASCIICraft/ecs/systems/MovementSystem.hpp>

#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/managers/PlayerManager.hpp>
#include <ASCIICraft/util/Util.hpp>

namespace ecs::systems {

MovementSystem::MovementSystem(entt::registry &registry) noexcept
  : m_registry(registry)
{}

void MovementSystem::Update() {
    ProcessMovementInput();
}

void MovementSystem::ProcessMovementInput() {
    const auto &input = ASCIIgL::InputManager::GetInst();

    if (!m_registry.ctx().contains<managers::PlayerManager>()) return;
    auto &pm = m_registry.ctx().get<managers::PlayerManager>();
    auto p_ent = pm.getPlayerEnt();
    if (p_ent == entt::null || !m_registry.valid(p_ent)) return;

    // required components
    if (!m_registry.all_of<
        components::PlayerCamera,
        components::PlayerController,
        components::Jump,
        components::Velocity,
        components::GroundPhysics,
        components::FlyingPhysics,
        components::Gravity,
        components::Transform,
        components::PlayerMode>(p_ent)) return;

    auto &cam = m_registry.get<components::PlayerCamera>(p_ent);
    auto &ctrl = m_registry.get<components::PlayerController>(p_ent);
    auto &jump = m_registry.get<components::Jump>(p_ent);
    auto &vel = m_registry.get<components::Velocity>(p_ent);
    auto &groundPhy = m_registry.get<components::GroundPhysics>(p_ent);
    auto &flyingPhy = m_registry.get<components::FlyingPhysics>(p_ent);
    auto &grav = m_registry.get<components::Gravity>(p_ent);
    auto &tx = m_registry.get<components::Transform>(p_ent);
    auto &pmode = m_registry.get<components::PlayerMode>(p_ent);

    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    const float currentTime = NowSeconds();

    // --- tuning constants (tweak to taste) ---
    const float walkSpeed = 4.3f;        // blocks/sec
    const float sprintSpeed = 5.6f;
    const float sneakSpeed = 1.3f;
    const float groundAccel = 50.0f;     // how fast we approach target on ground
    const float airAccel = 10.0f;        // reduced control in air

    // --- input -> desired horizontal direction (yaw only) ---
    glm::vec3 forward = cam.camera->getCamFrontNoY();
    glm::vec3 right = cam.camera->getCamRightNoY();
    glm::vec3 moveDir(0.0f);

    if (input.IsActionHeld("move_forward"))  moveDir += forward;
    if (input.IsActionHeld("move_backward")) moveDir -= forward;
    if (input.IsActionHeld("move_left"))     moveDir -= right;
    if (input.IsActionHeld("move_right"))    moveDir += right;

    // normalize horizontal input
    glm::vec2 moveXZ = glm::vec2(moveDir.x, moveDir.z);
    float moveLen = glm::length(moveXZ);
    if (moveLen > 1e-6f) moveXZ /= moveLen;

    // determine target speed
    if (input.IsActionHeld("sprint") && moveLen > 0.01f && !(ctrl.movementState == MovementState::Sneaking) && pmode.gamemode != GameMode::Spectator) {
        ctrl.movementState = MovementState::Running;
    } else if (input.IsActionHeld("sneak") && !ctrl.isFlying() && (pmode.gamemode != GameMode::Spectator)) {
        ctrl.movementState = MovementState::Sneaking;
    } else {
        ctrl.movementState = MovementState::Walking;
    }

    float targetSpeed = ctrl.isSneaking() ? sneakSpeed : (ctrl.isRunning() ? sprintSpeed : walkSpeed);

    // desired horizontal velocity in world space
    glm::vec3 desiredHoriz = glm::vec3(moveXZ.x * targetSpeed, 0.0f, moveXZ.y * targetSpeed);

    // --- update jump buffer timer (early presses) ---
    jump.jumpBufferTimer = std::max(0.0f, jump.jumpBufferTimer - dt);
    if (input.IsActionPressed("jump")) {
        jump.jumpBufferTimer = jump.JUMP_BUFFER_MAX;
    }

    // --- update lastOnGround timestamp from ground check (physics should set onGround) ---
    if (groundPhy.onGround) {
        groundPhy.lastOnGround = currentTime;
    }

    // --- horizontal acceleration: approach current toward desired ---
    glm::vec3 currentHoriz = glm::vec3(vel.linear.x, 0.0f, vel.linear.z);
    float accel = groundPhy.onGround ? groundAccel : airAccel;
    glm::vec3 delta = desiredHoriz - currentHoriz;
    float maxStep = accel * dt;
    float deltaLen = glm::length(delta);
    if (deltaLen > maxStep && deltaLen > 0.0f) delta = delta * (maxStep / deltaLen);
    currentHoriz += delta;

    // write back horizontal velocity
    vel.linear.x = currentHoriz.x;
    vel.linear.z = currentHoriz.z;

    // clamp horizontal speed to target (prevents diagonal overspeed)
    float horizSpeed2 = currentHoriz.x * currentHoriz.x + currentHoriz.z * currentHoriz.z;
    if (targetSpeed > 0.0f && horizSpeed2 > targetSpeed * targetSpeed) {
        glm::vec2 clamped = glm::normalize(glm::vec2(currentHoriz.x, currentHoriz.z)) * targetSpeed;
        vel.linear.x = clamped.x;
        vel.linear.z = clamped.y;
    }

    // --- jump logic using lastOnGround for coyote time and jumpBufferTimer for buffering ---
    bool coyoteAllowed = (currentTime - groundPhy.lastOnGround) <= jump.COYOTE_TIME_MAX;
    bool canJumpNow = (groundPhy.onGround || coyoteAllowed) && jump.jumpCooldown <= 0.0f && !ctrl.isFlying();
    bool bufferedJump = jump.jumpBufferTimer > 0.0f;

    if (bufferedJump && canJumpNow) {
        float g = std::abs(grav.acceleration.y);
        float jumpVelocity = std::sqrt(2.0f * g * jump.jumpHeight);
        vel.linear.y = jumpVelocity;

        // optional sprint forward boost
        if (ctrl.isRunning()) {
            glm::vec3 fwd = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
            vel.linear.x += fwd.x * 0.2f * targetSpeed;
            vel.linear.z += fwd.z * 0.2f * targetSpeed;
        }

        groundPhy.onGround = false;
        jump.jumpCooldown = jump.jumpCooldownMax;
        jump.jumpBufferTimer = 0.0f; // consume buffer
    }

    // spectator / flying vertical control
    if (ctrl.isFlying() || pmode.gamemode == GameMode::Spectator) {
        if (input.IsActionHeld("jump")) vel.linear.y = targetSpeed;
        else if (input.IsActionHeld("sneak")) vel.linear.y = -targetSpeed;
        else vel.linear.y = 0.0f;
    }
}

}