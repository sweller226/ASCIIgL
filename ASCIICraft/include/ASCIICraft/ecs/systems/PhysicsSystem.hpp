// ecs/systems/PhysicsSystem.hpp
#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

class World;

namespace blockstate {
class BlockStateRegistry;
} // namespace blockstate

namespace ecs::systems {

class PhysicsSystem : public ISystem {
public:
    explicit PhysicsSystem(entt::registry &registry);

    /// Call every frame with frame dt; system accumulates and steps at fixed tick
    void Update() override;

private:
    struct VoxelOverlapProbe {
        const World *world = nullptr;
        const blockstate::BlockStateRegistry *bsr = nullptr;
        glm::vec3 halfExtents{};
        bool colliderDisabled = false;

        bool operator()(const glm::vec3 &center) const;
    };

    void Step(float fixedDt);
    void IntegrateEntities(float dt, const World &world);

    void ResolveAABBAgainstWorld(
        components::Transform &t,
        components::Collider &col,
        components::Velocity &vel,
        float dt,
        const World &world,
        const blockstate::BlockStateRegistry *bsr,
        const components::StepPhysics *stepPhysics,
        components::GroundPhysics *groundPhysics,
        bool isSneaking
    );

    void UpdateGroundState(
        const glm::vec3 &pos,
        const glm::vec3 &halfExtents,
        components::Velocity &vel,
        const VoxelOverlapProbe &overlaps,
        components::GroundPhysics *groundPhysics
    );

    bool HasGroundSupport(
        const glm::vec3 &pos,
        const VoxelOverlapProbe &overlaps
    ) const;

    void ClampSneakEdge(
        const glm::vec3 &preHorizPos,
        glm::vec3 &pos,
        components::Velocity &vel,
        const VoxelOverlapProbe &overlaps
    ) const;

    float BinarySearchSupport(
        const glm::vec3 &startPos,
        const glm::vec3 &direction,
        float maxDistance,
        const VoxelOverlapProbe &overlaps
    ) const;

    bool TryStepUp(
        const components::Velocity &vel,
        float stepHeight,
        float dt,
        glm::vec3 &currentPos,
        const VoxelOverlapProbe &overlaps
    );

    void SlideHorizontal(
        glm::vec3 &pos,
        components::Velocity &vel,
        float dt,
        const VoxelOverlapProbe &overlaps
    );

    float BinarySearchCollision(
        const glm::vec3 &startPos,
        const glm::vec3 &direction,
        float maxDistance,
        const VoxelOverlapProbe &overlaps
    ) const;

    entt::registry &m_registry;
    float m_accumulator{0.0f};

    static constexpr float FixedDt = 1.0f / 30.0f;
    static constexpr float AIR_FRICTION = 1.0f;
    static constexpr float GROUND_PROBE_DISTANCE = 0.05f;
    static constexpr float MOVE_EPSILON = 0.0001f;
};

} // namespace ecs::systems
