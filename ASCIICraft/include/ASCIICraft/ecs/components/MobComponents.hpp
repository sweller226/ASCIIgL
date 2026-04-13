#pragma once
// =========================================================================
// MobComponents.hpp — ECS components for the mob AI system
//
// These are lightweight data-only structs attached to mob entities.
// The MobAISystem reads/writes them each frame to drive behavior.
//
// Component overview:
//   MobTag       — Type identification (typeId maps to MobFactory's type table)
//   Health       — HP tracking for damage and death
//   MobCategory  — Passive vs Monster (determines which goals are registered)
//   HurtState    — Last damage source (triggers PanicGoal, cleared after 5s)
//   AttackTarget — Current entity to chase/attack (set by FindTargetGoal)
//   MobAI        — AI brain: owns the two AITaskSchedulers (behavior + target)
//   DeathState   — Death animation timer and despawn delay
//   CreeperSwell — Tracks swell charge / explosion state for Creeper
// =========================================================================

#include <cstdint>
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace ai { class AITaskScheduler; }

namespace ecs::components {

// Strongly-typed mob IDs — replaces raw uint32_t magic numbers.
// Values must stay in sync with MobFactory::init() registrations.
enum class MobType : uint32_t {
    Pig      = 1,
    Chicken  = 2,
    Cow      = 3,
    Creeper  = 4,
    Sheep    = 5,
    Skeleton = 6,
    Spider   = 7,
    Zombie   = 8,
    Ocelot   = 9,
    Siamese  = 10,
    BlackCat = 11,
};

// Tag component to mark an entity as a mob and store its type id.
struct MobTag {
    MobType typeId = MobType::Pig;
};

// Basic health component
struct Health {
    int hp = 10;
    int maxHp = 10;
};

// Mob category determines base behavior template
enum class MobCategory : uint8_t {
    Passive,    // pig, cow, chicken, sheep, ocelot
    Monster     // zombie, skeleton, creeper, spider
};

// Tracks last damage source for panic/retaliation
struct HurtState {
    bool wasHurt = false;
    float hurtTimer = 0.0f;     // seconds since last hurt
    entt::entity attacker = entt::null;
};

// Attack target for hostile mobs
struct AttackTarget {
    entt::entity target = entt::null;
};

// AI brain — owns the goal-based schedulers.
// Two schedulers per mob (mirrors MC EntityAITasks):
//   tasks       — behavior goals (wander, panic, attack, etc.)
//   targetTasks — target selection goals (FindTargetGoal)
struct MobAI {
    std::unique_ptr<ai::AITaskScheduler> tasks;       // behavior scheduler
    std::unique_ptr<ai::AITaskScheduler> targetTasks;  // target selection scheduler
    MobCategory category = MobCategory::Passive;

    // Movement state set by AI goals
    glm::vec3 moveTarget{0.0f};
    bool hasMoveTo = false;
    float currentSpeed = 0.0f;

    // Look state set by AI goals
    float targetYaw = 0.0f;
    bool hasLookTarget = false;
};

// Death state for despawning.
// When HP <= 0, MobAISystem sets isDead=true and begins the death animation
// (tilt Z 90° over despawnDelay seconds), then destroys the entity.
struct DeathState {
    bool isDead = false;
    float deathTimer = 0.0f;       // time since death (for animation/fade)
    float despawnDelay = 1.5f;     // seconds before entity removal
};

// Creeper swell state — managed by CreeperSwellGoal.
// swellProgress [0,1]: 0=idle, 1=fully charged, triggers explosion.
struct CreeperSwell {
    float swellProgress = 0.0f; // 0 = idle, 1 = explode
    bool  isSwelling    = false;
};

// Fall damage tracking.
struct FallState {
    float maxY  = 0.0f;   // highest Y reached since leaving ground
    bool  inAir = false;
};

} // namespace ecs::components
