#pragma once

#include <glm/glm.hpp>

namespace ecs::components {

struct ParticleMovement {
    glm::vec3 velocity     = {};
    glm::vec3 acceleration = {};  // e.g. gravity = {0, -9.8f, 0}
    float     drag         = 0.0f; // 0 = no drag, 1 = full stop
};

}