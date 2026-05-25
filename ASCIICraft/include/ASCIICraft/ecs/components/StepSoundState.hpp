#pragma once

#include <glm/vec3.hpp>

namespace ecs::components {

struct StepSoundState {
    float distanceAccum = 0.0f;
    float cooldown      = 0.0f;
    glm::vec3 lastPosition{0.0f};
    bool      wasOnGround = false;
};

} // namespace ecs::components