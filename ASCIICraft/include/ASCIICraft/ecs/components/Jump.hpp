#pragma once

namespace ecs::components {

struct Jump {
    float jumpHeight = 1.35f;              // Blocks (matches Minecraft)

    // Timers
    float jumpCooldown = 0.0f;             // Current cooldown remaining
    float jumpCooldownMax = 0.2f;          // Cooldown after jump
    float jumpBufferTimer = 0.0f;          // Input buffer timer

    static constexpr float COYOTE_TIME_MAX = 0.12f;   // seconds
    static constexpr float JUMP_BUFFER_MAX = 0.12f;   // seconds
};

}