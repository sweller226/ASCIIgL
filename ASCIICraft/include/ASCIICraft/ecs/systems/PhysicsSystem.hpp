// ecs/systems/PhysicsSystem.hpp
#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <functional>

#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>

namespace ecs::systems {

class PhysicsSystem {
public:
    explicit PhysicsSystem(entt::registry &registry) noexcept;
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    // Call every frame with frame dt; system accumulates and steps at fixed tick
    void Update(float frameDt);

    // Run a single fixed physics step
    void Step(float fixedDt);

private:
    entt::registry &m_registry;

    // Accumulator for fixed-step integration
    float m_accumulator{0.0f};

    // Fixed tick rate (20 ticks/sec like Minecraft)
    static constexpr float FixedDt = 1.0f / 20.0f;

    // Integration
    void IntegrateEntities(float dt);
    
    // Collision resolution (generic for any entity)
    void ResolveAABBAgainstWorld(entt::entity ent, 
                                 ecs::components::Transform &t,
                                 ecs::components::Collider &col, 
                                 ecs::components::Velocity &vel,
                                 float dt,
                                 ecs::components::StepPhysics *stepPhysics,
                                 ecs::components::GroundPhysics *groundPhysics);
    
    // Step-up logic (for any entity with StepPhysics)
    bool TryStepUp(ecs::components::Transform &t, 
                   ecs::components::Collider &col,
                   ecs::components::Velocity &vel, 
                   float stepHeight, 
                   float dt,
                   glm::vec3 &currentPos);
    
    // Slide along collision surfaces horizontally
    void SlideHorizontal(glm::vec3 &pos, 
                        ecs::components::Velocity &vel, 
                        float dt,
                        const std::function<bool(const glm::vec3&)> &overlapsVoxel);
    
    // Binary search to find safe movement distance
    float BinarySearchCollision(const glm::vec3 &startPos, 
                               const glm::vec3 &direction,
                               float maxDistance,
                               const std::function<bool(const glm::vec3&)> &overlapsVoxel);
};

} // namespace ecs::systems