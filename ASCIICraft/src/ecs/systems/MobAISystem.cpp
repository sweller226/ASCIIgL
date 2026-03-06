// =========================================================================
// MobAISystem.cpp — Mob AI initialization and per-frame update
//
// Each mob type gets a tailored set of AI goals at specific priorities.
// Passive mobs: PanicGoal > WanderGoal > LookIdleGoal
// Hostile mobs: Attack/KeepDistance > WanderGoal > LookIdleGoal
//               + FindTargetGoal on the target scheduler
//
// Speeds are in blocks/second (no MC multiplier).  For reference:
//   MC player walk = 4.317 bps.  Mobs are typically 0.8–1.3 bps wander,
//   1.2–2.2 bps chase/panic.
//
// Death sequence: HP <= 0 → isDead → tilt Z 90° over 1.5s → entity destroyed.
// Render-distance despawn: horizontal dist > renderDistance * 16 blocks.
// =========================================================================

#include <ASCIICraft/ecs/systems/MobAISystem.hpp>
#include <ASCIICraft/ecs/components/MobComponents.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ai/AIGoal.hpp>
#include <ASCIICraft/ai/AIBehaviors.hpp>
#include <ASCIICraft/ai/Pathfinding.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/ChunkManager.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <memory>

namespace ecs::systems {

MobAISystem::MobAISystem(entt::registry& registry)
    : m_registry(registry) {}

const ChunkManager* MobAISystem::getChunkManager() const {
    World* w = GetWorldPtr(const_cast<entt::registry&>(m_registry));
    if (!w) return nullptr;
    return w->GetChunkManager();
}

// =====================================================================
// Initialize AI brain for a single mob
// Sets up goals based on mob type (passive vs monster)
// =====================================================================
void MobAISystem::initializeMob(entt::entity e) {
    auto* mob = m_registry.try_get<components::MobTag>(e);
    if (!mob) return;

    const auto* cm = getChunkManager();
    if (!cm) return;
    auto& bsr = m_registry.ctx().get<blockstate::BlockStateRegistry>();

    // Create AI brain
    auto& ai = m_registry.get_or_emplace<components::MobAI>(e);
    ai.tasks = std::make_unique<::ai::AITaskScheduler>();
    ai.targetTasks = std::make_unique<::ai::AITaskScheduler>();

    // Ensure required components exist
    (void)m_registry.get_or_emplace<components::HurtState>(e);
    (void)m_registry.get_or_emplace<components::DeathState>(e);
    (void)m_registry.get_or_emplace<components::Health>(e);

    // Determine category and register goals based on mob type
    uint32_t typeId = mob->typeId;

    // Type mapping from MobFactory:
    // 1=Pig, 2=Chicken, 3=Cow, 4=Creeper, 5=Sheep,
    // 6=Skeleton, 7=Spider, 8=Zombie, 9=Ocelot, 10=Siamese, 11=Black Cat

    switch (typeId) {
        // =============================================================
        // Speed reference (blocks per second):
        //   MC player walk = 4.317 bps.  MC mobs are MUCH slower.
        //   Zombie  ~1.0 bps wander, ~1.4 bps chase
        //   Skeleton ~1.0 bps wander, ~1.2 bps strafe
        //   Spider  ~1.3 bps wander, ~1.7 bps chase
        //   Creeper ~1.0 bps wander, ~1.4 bps chase
        //   Passive ~0.8-1.0 bps wander, ~1.5-2.0 bps panic
        // =============================================================

        case 1: // Pig
            ai.category = components::MobCategory::Passive;
            ai.moveSpeed = 1.0f;
            ai.tasks->addGoal(1, std::make_unique<::ai::PanicGoal>(m_registry, e, cm, bsr, 1.8f));
            ai.tasks->addGoal(6, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.0f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            break;

        case 2: // Chicken
            ai.category = components::MobCategory::Passive;
            ai.moveSpeed = 1.0f;
            ai.tasks->addGoal(1, std::make_unique<::ai::PanicGoal>(m_registry, e, cm, bsr, 2.0f));
            ai.tasks->addGoal(6, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 0.9f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            break;

        case 3: // Cow
            ai.category = components::MobCategory::Passive;
            ai.moveSpeed = 0.8f;
            ai.tasks->addGoal(1, std::make_unique<::ai::PanicGoal>(m_registry, e, cm, bsr, 1.5f));
            ai.tasks->addGoal(6, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 0.8f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            break;

        case 5: // Sheep
            ai.category = components::MobCategory::Passive;
            ai.moveSpeed = 0.9f;
            ai.tasks->addGoal(1, std::make_unique<::ai::PanicGoal>(m_registry, e, cm, bsr, 1.6f));
            ai.tasks->addGoal(6, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 0.9f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            break;

        case 9:  // Ocelot
        case 10: // Siamese
        case 11: // Black Cat
            ai.category = components::MobCategory::Passive;
            ai.moveSpeed = 1.2f;
            ai.tasks->addGoal(1, std::make_unique<::ai::PanicGoal>(m_registry, e, cm, bsr, 2.2f));
            ai.tasks->addGoal(6, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.0f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            break;

        case 4: // Creeper
            ai.category = components::MobCategory::Monster;
            ai.moveSpeed = 1.0f;
            ai.tasks->addGoal(4, std::make_unique<::ai::MeleeAttackGoal>(m_registry, e, cm, bsr, 1.4f, 0.0f)); // Creeper doesn't melee, but chases
            ai.tasks->addGoal(5, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.0f));
            ai.tasks->addGoal(6, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            ai.targetTasks->addGoal(1, std::make_unique<::ai::FindTargetGoal>(m_registry, e, 16.0f));
            break;

        case 6: // Skeleton — keeps distance, strafes, doesn't hard-chase
            ai.category = components::MobCategory::Monster;
            ai.moveSpeed = 1.0f;
            ai.tasks->addGoal(3, std::make_unique<::ai::KeepDistanceGoal>(m_registry, e, cm, bsr, 1.2f, 8.0f, 14.0f));
            ai.tasks->addGoal(5, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.0f));
            ai.tasks->addGoal(6, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            ai.targetTasks->addGoal(1, std::make_unique<::ai::FindTargetGoal>(m_registry, e, 16.0f));
            break;

        case 7: // Spider
            ai.category = components::MobCategory::Monster;
            ai.moveSpeed = 1.3f;
            ai.tasks->addGoal(4, std::make_unique<::ai::MeleeAttackGoal>(m_registry, e, cm, bsr, 1.7f, 2.0f));
            ai.tasks->addGoal(5, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.3f));
            ai.tasks->addGoal(6, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            ai.targetTasks->addGoal(1, std::make_unique<::ai::FindTargetGoal>(m_registry, e, 16.0f));
            break;

        case 8: // Zombie
            ai.category = components::MobCategory::Monster;
            ai.moveSpeed = 1.0f;
            ai.tasks->addGoal(2, std::make_unique<::ai::MeleeAttackGoal>(m_registry, e, cm, bsr, 1.4f, 3.0f));
            ai.tasks->addGoal(7, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.0f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            ai.targetTasks->addGoal(1, std::make_unique<::ai::FindTargetGoal>(m_registry, e, 40.0f));
            break;

        default:
            // basic passive behavior
            ai.category = components::MobCategory::Passive;
            ai.moveSpeed = 0.25f;
            ai.tasks->addGoal(6, std::make_unique<::ai::WanderGoal>(m_registry, e, cm, bsr, 1.0f));
            ai.tasks->addGoal(8, std::make_unique<::ai::LookIdleGoal>(m_registry, e));
            break;
    }

    ASCIIgL::Logger::Debug("Initialized AI for mob type " + std::to_string(typeId));
}

void MobAISystem::initializeMobs() {
    auto view = m_registry.view<components::MobTag>();
    for (auto e : view) {
        // Only initialize if no AI brain yet
        auto* ai = m_registry.try_get<components::MobAI>(e);
        if (ai && ai->tasks) continue;
        initializeMob(e);
    }
}

// =====================================================================
// Main update loop
// =====================================================================
void MobAISystem::update(float dt) {
    // 1. Update hurt timers
    updateHurtTimers(dt);

    // 2. Run AI schedulers for all mobs
    auto view = m_registry.view<components::MobTag, components::MobAI>();
    for (auto [e, mob, ai] : view.each()) {
        // Skip dead mobs
        auto* death = m_registry.try_get<components::DeathState>(e);
        if (death && death->isDead) continue;

        // Run target selection AI
        if (ai.targetTasks) {
            ai.targetTasks->update(dt);
        }

        // Run behavior AI
        if (ai.tasks) {
            ai.tasks->update(dt);
        }
    }

    // 3. Push overlapping mobs apart (entity-entity collision)
    pushMobsApart();

    // 4. Death and despawn
    updateDeathAndDespawn(dt);

    // 5. Despawn mobs outside render distance from player
    despawnOutOfRange();
}

// =====================================================================
// Despawn mobs that are beyond the world's render distance from the player
// =====================================================================
void MobAISystem::despawnOutOfRange() {
    const auto* cm = getChunkManager();
    if (!cm) return;

    // Render distance in blocks (chunks * 16)
    float despawnDist = static_cast<float>(cm->GetRenderDistance() * 16);
    float despawnDistSq = despawnDist * despawnDist;

    // Find the player position
    glm::vec3 playerPos(0.0f);
    bool foundPlayer = false;
    {
        auto playerView = m_registry.view<components::PlayerTag, components::Transform>();
        for (auto [e, pt] : playerView.each()) {
            playerPos = pt.position;
            foundPlayer = true;
            break;
        }
    }
    if (!foundPlayer) return;

    // Check all mobs
    std::vector<entt::entity> toDestroy;
    auto mobView = m_registry.view<components::MobTag, components::Transform>();
    for (auto [e, mob, t] : mobView.each()) {
        glm::vec3 diff = t.position - playerPos;
        diff.y = 0.0f; // horizontal distance only
        float distSq = glm::dot(diff, diff);
        if (distSq > despawnDistSq) {
            toDestroy.push_back(e);
        }
    }

    for (auto e : toDestroy) {
        ASCIIgL::Logger::Debug("Mob despawned (out of render distance)");
        m_registry.destroy(e);
    }
}

// =====================================================================
// Entity-entity push — MC-style mob collision avoidance.
// O(n²) pairwise check using horizontal distance and collider radii.
// Mobs within overlapping radius push apart by a small fraction of the
// overlap each frame, resulting in smooth separation.
// For exactly-overlapping mobs, a deterministic direction based on
// entity ID prevents both mobs from being pushed the same way.
// =====================================================================
void MobAISystem::pushMobsApart() {
    // Collect all mob positions + colliders
    struct MobInfo {
        entt::entity entity;
        glm::vec3 pos;
        float radius; // horizontal collision radius
    };

    std::vector<MobInfo> mobs;
    auto view = m_registry.view<components::MobTag, components::Transform>();
    for (auto [e, mob, t] : view.each()) {
        // Skip dead mobs
        auto* death = m_registry.try_get<components::DeathState>(e);
        if (death && death->isDead) continue;

        float radius = 0.3f; // default half-extent
        auto* col = m_registry.try_get<components::Collider>(e);
        if (col) {
            radius = std::max(col->halfExtents.x, col->halfExtents.z);
        }
        mobs.push_back({e, t.position, radius});
    }

    // O(n^2) push — fine for small mob counts
    constexpr float PUSH_STRENGTH = 0.05f; // nudge strength per frame
    for (size_t i = 0; i < mobs.size(); ++i) {
        for (size_t j = i + 1; j < mobs.size(); ++j) {
            glm::vec3 diff = mobs[i].pos - mobs[j].pos;
            diff.y = 0.0f; // horizontal only

            float dist = glm::length(diff);
            float minDist = mobs[i].radius + mobs[j].radius;

            if (dist < minDist && dist > 0.001f) {
                // Push apart proportional to overlap
                glm::vec3 pushDir = diff / dist;
                float overlap = minDist - dist;
                float pushAmount = overlap * PUSH_STRENGTH;

                // Apply push via direct position offset (MC-style)
                auto* ti = m_registry.try_get<components::Transform>(mobs[i].entity);
                auto* tj = m_registry.try_get<components::Transform>(mobs[j].entity);
                if (ti) ti->setPosition(ti->position + pushDir * pushAmount);
                if (tj) tj->setPosition(tj->position - pushDir * pushAmount);
            } else if (dist <= 0.001f) {
                // Exactly overlapping — push in random-ish direction based on entity ids
                float angle = static_cast<float>(static_cast<uint32_t>(mobs[i].entity)) * 0.7f;
                glm::vec3 pushDir(std::cos(angle), 0.0f, std::sin(angle));
                auto* ti = m_registry.try_get<components::Transform>(mobs[i].entity);
                auto* tj = m_registry.try_get<components::Transform>(mobs[j].entity);
                if (ti) ti->setPosition(ti->position + pushDir * PUSH_STRENGTH);
                if (tj) tj->setPosition(tj->position - pushDir * PUSH_STRENGTH);
            }
        }
    }
}

// =====================================================================
// Hurt timer management and visual feedback
// =====================================================================
void MobAISystem::updateHurtTimers(float dt) {
    auto view = m_registry.view<components::HurtState>();
    for (auto [e, hurt] : view.each()) {
        if (hurt.wasHurt) {
            hurt.hurtTimer += dt;

            // Visual feedback: rapid blink for the first 0.5 seconds after being hit.
            // Toggle visibility at ~10 Hz (every 0.1s) so the player can clearly
            // see which mob took damage.  After the blink window, ensure visible.
            constexpr float BLINK_DURATION = 0.5f;
            constexpr float BLINK_RATE     = 0.1f;  // seconds per on/off cycle
            auto* rend = m_registry.try_get<components::Renderable>(e);
            if (rend) {
                if (hurt.hurtTimer <= BLINK_DURATION) {
                    // Integer division of timer by blink rate: even = visible, odd = hidden
                    int phase = static_cast<int>(hurt.hurtTimer / BLINK_RATE);
                    rend->visible = (phase % 2 == 0);
                } else {
                    rend->visible = true;  // ensure visible after blink ends
                }
            }

            // Clear hurt state after 5 seconds
            if (hurt.hurtTimer > 5.0f) {
                hurt.wasHurt = false;
                hurt.hurtTimer = 0.0f;
                hurt.attacker = entt::null;
            }
        }
    }
}

// =====================================================================
// Death detection and despawn
// =====================================================================
void MobAISystem::updateDeathAndDespawn(float dt) {
    // Check for mobs that should die (HP <= 0)
    {
        auto view = m_registry.view<components::Health, components::DeathState>();
        for (auto [e, health, death] : view.each()) {
            if (!death.isDead && health.hp <= 0) {
                death.isDead = true;
                death.deathTimer = 0.0f;

                // Stop AI
                auto* ai = m_registry.try_get<components::MobAI>(e);
                if (ai) {
                    if (ai->tasks) ai->tasks->stopAll();
                    if (ai->targetTasks) ai->targetTasks->stopAll();
                }

                // Stop movement
                auto* vel = m_registry.try_get<components::Velocity>(e);
                if (vel) vel->linear = glm::vec3(0.0f);

                ASCIIgL::Logger::Debug("Mob died");
            }
        }
    }

    // Update death timers and despawn
    {
        std::vector<entt::entity> toDestroy;
        auto view = m_registry.view<components::DeathState>();
        for (auto [e, death] : view.each()) {
            if (!death.isDead) continue;

            death.deathTimer += dt;

            // Visual feedback: tilt the mob over as it dies
            if (death.deathTimer < death.despawnDelay) {
                auto* t = m_registry.try_get<components::Transform>(e);
                if (t) {
                    float progress = death.deathTimer / death.despawnDelay;
                    float tiltAngle = progress * 1.5708f; // 90 degrees
                    // Rotate around Z axis to "fall over"
                    glm::quat deathRot = glm::angleAxis(tiltAngle, glm::vec3(0, 0, 1));
                    t->setRotation(deathRot);
                }
            }

            // Despawn after delay
            if (death.deathTimer >= death.despawnDelay) {
                toDestroy.push_back(e);
            }
        }

        for (auto e : toDestroy) {
            m_registry.destroy(e);
        }
    }
}

} // namespace ecs::systems
