// ecs/components/PlayerController.hpp
#pragma once
#include <cstdint>

namespace ecs::components {

struct PlayerController {
    float move_x = 0.f;      // -1..1 (right)
    float move_y = 0.f;      // -1..1 (forward)
    bool jumpPressed = false;
    bool sprinting = false;
    bool sneaking = false;
    bool flying = false;
    bool input_enabled = true;

    // Timers
    float jumpCooldown;

};

}
