// ecs/components/PlayerController.hpp
#pragma once
#include <cstdint>

enum class MovementState {
    Walking,
    Running,
    Sneaking,
    Flying,
};

namespace ecs::components {

struct PlayerController {
    MovementState movementState;
    
    float move_x = 0.f;      // -1..1 (right)
    float move_y = 0.f;      // -1..1 (forward)
    bool input_enabled = true;

    float walkSpeed;
    float runSpeed;
    float sneakSpeed;
    float flySpeed;

    // Remaining time after a Space press during which a second press toggles creative flight.
    float flyToggleTimer = 0.0f;
    // After enabling creative flight, skip land-cancel briefly so ground contact does not immediately disable it.
    float flyLandGrace = 0.0f;

    static constexpr float JUMP_COOLDOWN_MAX = 0.2f;         // Seconds
    static constexpr float DEFAULT_WALK_SPEED = 4.317f;      // Blocks per second
    static constexpr float RUN_SPEED = 5.612f;       // Blocks per second
    static constexpr float WALK_SPEED = 4.3f;
    static constexpr float SNEAK_SPEED = 1.295f;     // Blocks per second
    static constexpr float SPECTATOR_FLY_SPEED = 20.0f;                // Spectator
    static constexpr float CREATIVE_FLY_SPEED = 8.0f;
    static constexpr float CREATIVE_FLY_SPRINT_SPEED = 15.0f;
    static constexpr float FLY_TOGGLE_WINDOW = 0.3f;         // ~7 ticks
    static constexpr float FLY_LAND_GRACE = 0.15f;
    static constexpr float JUMP_HEIGHT = 1.35f;      // Blocks

    static constexpr float GROUND_ACCEL = 50.0f;
    static constexpr float AIR_ACCEL = 50.0f;

    // True while sprinting on foot or Ctrl-sprinting during creative flight (drives FOV, etc.).
    bool sprinting = false;

    bool isWalking() { return movementState == MovementState::Walking; }
    bool isRunning() { return movementState == MovementState::Running; }
    bool isSneaking() { return movementState == MovementState::Sneaking; }
    bool isFlying() { return movementState == MovementState::Flying; }
};

}
