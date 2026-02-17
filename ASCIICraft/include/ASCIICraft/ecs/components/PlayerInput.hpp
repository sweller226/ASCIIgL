#pragma once

#include <glm/glm.hpp>

namespace ecs::components {

struct PlayerInput {
    glm::vec3 moveDirection {0.0f, 0.0f, 0.0f};
    glm::vec2 lookDelta {0.0f, 0.0f};

    bool wantsToSprint = false;
    bool wantsToSneak  = false;
    bool wantsToJump   = false;
    bool wantsToFlyUp  = false;
    bool wantsToFlyDown = false;
};

} // namespace ecs::components
