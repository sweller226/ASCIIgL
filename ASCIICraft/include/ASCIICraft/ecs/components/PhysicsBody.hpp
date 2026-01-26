// ecs/components/PhysicsMinecraft.hpp
#pragma once

#include <entt/entt.hpp>
#include <glm/vec3.hpp>
#include <cstdint>

//
// Minecraft-style physics components for an ECS voxel engine.
// Keep data POD-like; implement behavior in PhysicsSystem.
// Defaults approximate Minecraft conventions (player AABB ~0.6 x 1.8).
//

namespace ecs::components {

struct PhysicsBody {
    float mass{1.0f};
    float invMass{1.0f};
    float linearDamping{0.0f};
    bool enabled{true};

    void SetMass(float m) {
        mass = m;
        invMass = (m > 0.0f) ? (1.0f / m) : 0.0f;
    }
};

struct Collider {
    glm::vec3 halfExtents{0.3f, 0.9f, 0.3f};
    glm::vec3 localOffset{0.0f};
    uint32_t layer{1};
    uint32_t mask{0xFFFFFFFFu};
    bool disabled{false};
};

// --- Material tuning for block surfaces (slipperiness like ice) ---
struct PhysicsMaterial {
    float friction{0.6f};      // coefficient of friction (0..1+)
};

struct Gravity {
    glm::vec3 acceleration = glm::vec3(0.0f, -9.81f, 0.0f); // default downward gravity
    bool enabled{true};
};

struct StepPhysics {
    float stepHeight = 0.6f;  // Maximum step height (blocks/meters)
};

struct GroundPhysics {
    bool onGround = false;
    float lastOnGround = 0.0f;
};

struct FlyingPhysics {
    bool enabled = false;
};

} // namespace ecs::components