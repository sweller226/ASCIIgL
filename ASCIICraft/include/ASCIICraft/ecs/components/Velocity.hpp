// ecs/components/Velocity.hpp
#pragma once

#include <glm/glm.hpp>

namespace ecs::components {

struct Velocity {
    glm::vec3 linear{0.0f};         // current linear velocity (m/s)

    // Soft maximum speed. 0 means "no limit".
    float maxSpeed{0.0f};

    // Simple damping factor applied by movement/physics systems (0 = no damping).
    // This is a convenience value; actual damping application belongs in systems.
    float damping{0.0f};

    // Helpers kept inline and minimal so the component remains small.
    void ClampSpeed() {
        if (maxSpeed > 0.0f) {
            float len2 = glm::dot(linear, linear);
            float max2 = maxSpeed * maxSpeed;
            if (len2 > max2) {
                linear = glm::normalize(linear) * maxSpeed;
            }
        }
    }

    void ApplyDamping(float dt) {
        if (damping > 0.0f) {
            float factor = 1.0f / (1.0f + damping * dt);
            linear *= factor;
        }
    }
};

} // namespace ecs::components