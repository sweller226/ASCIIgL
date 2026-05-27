#pragma once

#include <cmath>

namespace ecs::components {

struct Jump {
    float jumpHeight = 1.35f;       // Blocks (matches Minecraft)
    float jumpVelocity = 0.0f;      // Cached from jumpHeight + gravity (see RefreshJumpVelocity)

    float jumpCooldown = 0.0f;
    static constexpr float JUMP_COOLDOWN_MAX = 0.5f;
    float bufferTime = 0.0f;
    float coyoteTime = 0.0f;

    static constexpr float COYOTE_TIME_MAX = 0.12f;
    static constexpr float JUMP_BUFFER_MAX = 0.12f;

    void RefreshJumpVelocity(float gravityY) {
        const float g = std::abs(gravityY);
        jumpVelocity = std::sqrt(2.0f * g * jumpHeight);
    }
};

} // namespace ecs::components
