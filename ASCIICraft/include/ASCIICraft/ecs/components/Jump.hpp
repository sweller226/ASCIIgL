namespace ecs::components {

struct Jump {
    float jumpHeight;

    // Timers
    float jumpCooldown;
    float jumpCooldownMax;
    float jumpBufferTimer;

    const float COYOTE_TIME_MAX = 0.12f;   // seconds
    const float JUMP_BUFFER_MAX = 0.12f;   // seconds
};

}