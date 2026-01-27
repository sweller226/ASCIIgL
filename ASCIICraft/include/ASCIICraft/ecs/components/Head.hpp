#pragma once

#include <glm/glm.hpp>

namespace ecs::components {

struct Head {
    glm::vec3 lookDir{0.0f, 0.0f, -1.0f};
    glm::vec3 relativePos{0.0f, 0.0f, 0.0f};
};

}

