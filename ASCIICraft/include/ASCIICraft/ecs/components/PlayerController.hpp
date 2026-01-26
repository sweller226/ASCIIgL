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

    static constexpr float JUMP_COOLDOWN_MAX = 0.2f;         // Seconds    static constexpr float DEFAULT_WALK_SPEED = 4.317f;      // Blocks per second
    static constexpr float RUN_SPEED = 5.612f;       // Blocks per second
    static constexpr float WALK_SPEED = 4.3f;
    static constexpr float SNEAK_SPEED = 1.295f;     // Blocks per second
    static constexpr float FLY_SPEED = 10.89f;       // Blocks per second
    static constexpr float JUMP_HEIGHT = 1.35f;      // Blocks

    static constexpr float GROUND_ACCEL = 50.0f;
    static constexpr float AIR_ACCEL = 50.0f;

    bool isWalking() { return movementState == MovementState::Walking; }
    bool isRunning() { return movementState == MovementState::Running; }
    bool isSneaking() { return movementState == MovementState::Sneaking; }
    bool isFlying() { return movementState == MovementState::Flying; }
};

}
