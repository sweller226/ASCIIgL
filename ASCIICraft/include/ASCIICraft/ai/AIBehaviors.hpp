#pragma once
// =========================================================================
// AIBehaviors.hpp — Concrete AI goal implementations for mob behavior
//
// Each goal class maps to a Minecraft 1.7.10 EntityAI* class.
// Goals are registered with AITaskScheduler at different priorities;
// the scheduler handles mutual exclusion via mutex bits.
//
// Goal overview:
//   WanderGoal       — Random idle wandering              (passive + hostile)
//   PanicGoal        — Flee when hurt                     (passive only)
//   LookIdleGoal     — Random head rotation when idle     (all mobs)
//   MeleeAttackGoal  — Chase target and attack on contact (zombie, spider)
//   KeepDistanceGoal — Strafe/maintain range from target  (skeleton)
//   FindTargetGoal   — Scan for nearest player            (all hostile)
//
// Helper functions:
//   findRandomTarget()     — pick a random block position near origin
//   findRandomTargetAway() — pick a position biased away from a threat
// =========================================================================

#include <ASCIICraft/ai/AIGoal.hpp>
#include <ASCIICraft/ai/Pathfinding.hpp>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <random>

// Forward declarations
class ChunkManager;
namespace blockstate { class BlockStateRegistry; }

namespace ai {

// =====================================================================
// Random position generation (MC RandomPositionGenerator equivalent).
// Produces block-coordinate targets for pathfinding destinations.
// =====================================================================
glm::ivec3 findRandomTarget(
    const glm::vec3& origin,
    int xzRange, int yRange,
    std::mt19937& rng);

glm::ivec3 findRandomTargetAway(
    const glm::vec3& origin,
    const glm::vec3& threat,
    int xzRange, int yRange,
    std::mt19937& rng);

// =====================================================================
// WanderGoal — Random wandering (passive + hostile idle movement)
// Mutex: MOVE (1)
// MC equivalent: EntityAIWander
// =====================================================================
class WanderGoal : public AIGoal {
public:
    WanderGoal(entt::registry& reg, entt::entity entity,
               const ChunkManager* cm,
               const blockstate::BlockStateRegistry& bsr,
               float speed = 1.0f, int cooldownTicks = 120);

    bool shouldExecute() override;
    bool continueExecuting() override;
    void startExecuting() override;
    void updateTask(float dt) override;
    void resetTask() override;

private:
    entt::registry& m_registry;
    entt::entity m_entity;
    const ChunkManager* m_cm;
    const blockstate::BlockStateRegistry& m_bsr;
    float m_speed;
    int m_cooldownTicks;
    Path m_path;
    std::mt19937 m_rng;
};

// =====================================================================
// PanicGoal — Flee when hurt (passive mob behavior)
// Mutex: MOVE (1)
// MC equivalent: EntityAIPanic
// =====================================================================
class PanicGoal : public AIGoal {
public:
    PanicGoal(entt::registry& reg, entt::entity entity,
              const ChunkManager* cm,
              const blockstate::BlockStateRegistry& bsr,
              float speed = 1.25f);

    bool shouldExecute() override;
    bool continueExecuting() override;
    void startExecuting() override;
    void updateTask(float dt) override;
    void resetTask() override;

private:
    entt::registry& m_registry;
    entt::entity m_entity;
    const ChunkManager* m_cm;
    const blockstate::BlockStateRegistry& m_bsr;
    float m_speed;
    Path m_path;
    std::mt19937 m_rng;
};

// =====================================================================
// LookIdleGoal — Randomly look around when idle
// Mutex: MOVE_LOOK (3)
// MC equivalent: EntityAILookIdle
// =====================================================================
class LookIdleGoal : public AIGoal {
public:
    LookIdleGoal(entt::registry& reg, entt::entity entity);

    bool shouldExecute() override;
    bool continueExecuting() override;
    void startExecuting() override;
    void updateTask(float dt) override;

private:
    entt::registry& m_registry;
    entt::entity m_entity;
    float m_lookTimer = 0.0f;
    float m_targetYaw = 0.0f;
    std::mt19937 m_rng;
};

// =====================================================================
// MeleeAttackGoal — Chase and attack target on collision
// Mutex: MOVE_LOOK (3)
// MC equivalent: EntityAIAttackOnCollide
// =====================================================================
class MeleeAttackGoal : public AIGoal {
public:
    MeleeAttackGoal(entt::registry& reg, entt::entity entity,
                    const ChunkManager* cm,
                    const blockstate::BlockStateRegistry& bsr,
                    float speed = 1.0f, float attackDamage = 3.0f);

    bool shouldExecute() override;
    bool continueExecuting() override;
    void startExecuting() override;
    void updateTask(float dt) override;
    void resetTask() override;

private:
    entt::registry& m_registry;
    entt::entity m_entity;
    const ChunkManager* m_cm;
    const blockstate::BlockStateRegistry& m_bsr;
    float m_speed;
    float m_attackDamage;
    float m_attackCooldown = 0.0f;
    float m_repathTimer = 0.0f;
    Path m_path;

    entt::entity findTarget() const;
};

// =====================================================================
// KeepDistanceGoal — Maintain distance from target (skeleton-style)
// Strafes around target, backs away if too close, approaches if too far.
// Unlike MeleeAttackGoal which charges in, this keeps the mob at range.
// Mutex: MOVE_LOOK (3)
// MC equivalent: EntityAIArrowAttack (movement portion)
// =====================================================================
class KeepDistanceGoal : public AIGoal {
public:
    KeepDistanceGoal(entt::registry& reg, entt::entity entity,
                     const ChunkManager* cm,
                     const blockstate::BlockStateRegistry& bsr,
                     float speed = 1.0f,
                     float preferredDist = 8.0f,
                     float maxDist = 14.0f);

    bool shouldExecute() override;
    bool continueExecuting() override;
    void startExecuting() override;
    void updateTask(float dt) override;
    void resetTask() override;

private:
    entt::registry& m_registry;
    entt::entity m_entity;
    const ChunkManager* m_cm;
    const blockstate::BlockStateRegistry& m_bsr;
    float m_speed;
    float m_preferredDist;      // ideal distance to maintain
    float m_maxDist;            // give up if target farther than this
    float m_strafeTimer = 0.0f; // timer to change strafe direction
    int m_strafeDir = 1;        // +1 = strafe right, -1 = strafe left
    std::mt19937 m_rng;

    entt::entity findTarget() const;
};

// =====================================================================
// FindTargetGoal — Find nearest player to attack (hostile mobs)
// Mutex: NONE (runs alongside other goals)
// MC equivalent: EntityAINearestAttackableTarget
// =====================================================================
class FindTargetGoal : public AIGoal {
public:
    FindTargetGoal(entt::registry& reg, entt::entity entity,
                   float range = 16.0f);

    bool shouldExecute() override;
    bool continueExecuting() override;
    void startExecuting() override;
    void resetTask() override;

private:
    entt::registry& m_registry;
    entt::entity m_entity;
    float m_range;
};

} // namespace ai
