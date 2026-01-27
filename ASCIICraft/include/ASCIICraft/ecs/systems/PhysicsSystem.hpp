// ecs/systems/PhysicsSystem.hpp
#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <functional>

#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>

namespace ecs::systems {

class PhysicsSystem {
public:
    explicit PhysicsSystem(entt::registry &registry) noexcept;
    
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    PhysicsSystem(PhysicsSystem&&) = delete;
    PhysicsSystem& operator=(PhysicsSystem&&) = delete;

    ~PhysicsSystem() = default;

    /// Call every frame with frame dt; system accumulates and steps at fixed tick
    void Update();

private:
    /// Run a single fixed physics step
    void Step(float fixedDt);

    /// Apply physics integration to all entities with Transform, Velocity, and Collider
    void IntegrateEntities(float dt);
    
    /// Resolve AABB collision against world voxels
    void ResolveAABBAgainstWorld(entt::entity ent, 
                                 components::Transform &t,
                                 components::Collider &col, 
                                 components::Velocity &vel,
                                 float dt,
                                 const components::StepPhysics *stepPhysics,
                                 components::GroundPhysics *groundPhysics);
    
    /// Update ground contact state and apply friction
    void UpdateGroundState(const glm::vec3 &pos,
                          const glm::vec3 &halfExtents,
                          components::Velocity &vel,
                          const std::function<bool(const glm::vec3&)> &overlapsVoxel,
                          components::GroundPhysics *groundPhysics);
    
    /// Attempt to step up obstacles (e.g., stairs, slabs)
    /// @return true if step-up succeeded, false otherwise
    bool TryStepUp(components::Transform &t, 
                   const components::Collider &col,
                   const components::Velocity &vel, 
                   float stepHeight, 
                   float dt,
                   glm::vec3 &currentPos);
    
    /// Slide along collision surfaces horizontally (X and Z axes)
    void SlideHorizontal(glm::vec3 &pos, 
                        components::Velocity &vel, 
                        float dt,
                        const std::function<bool(const glm::vec3&)> &overlapsVoxel);
    
    /// Binary search to find maximum safe movement distance before collision
    /// @return Maximum safe distance in range [0, maxDistance]
    [[nodiscard]] float BinarySearchCollision(
        const glm::vec3 &startPos, 
        const glm::vec3 &direction,
        float maxDistance,
        const std::function<bool(const glm::vec3&)> &overlapsVoxel) const;

    entt::registry &m_registry;
    float m_accumulator{0.0f};

    /// Fixed tick rate (30 ticks/sec like Minecraft)
    static constexpr float FixedDt = 1.0f / 30.0f;

    static constexpr float AIR_FRICTION = 1.0f;
};

} // namespace ecs::systems