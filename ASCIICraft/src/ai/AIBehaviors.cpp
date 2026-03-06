// =========================================================================
// AIBehaviors.cpp — Goal implementations and shared movement helpers
//
// Core helper: followPath()
//   All movement goals use this shared function to walk along A* paths.
//   It applies a flat-ground beeline optimization: since A* only generates
//   4-cardinal waypoints (N/S/E/W), following them literally causes ugly
//   stair-step zig-zag movement.  On flat ground (all waypoints at the
//   same Y level), the mob walks directly to the final destination instead.
//   Waypoint-by-waypoint following is only used when the path has Y changes
//   (steps up or drops down).
//
// Speed units: all speeds are in blocks/second (no MC 4.317 multiplier).
// Facing: models face -Z by default, so atan2(-dir.x, -dir.z) aligns them.
// =========================================================================

#include <ASCIICraft/ai/AIBehaviors.hpp>
#include <ASCIICraft/ecs/components/MobComponents.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/world/ChunkManager.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace ai {

// =====================================================================
// Random position generation
// =====================================================================
glm::ivec3 findRandomTarget(
    const glm::vec3& origin,
    int xzRange, int yRange,
    std::mt19937& rng)
{
    std::uniform_int_distribution<int> xzDist(-xzRange, xzRange);
    std::uniform_int_distribution<int> yDist(-yRange, yRange);
    return glm::ivec3(
        static_cast<int>(origin.x) + xzDist(rng),
        static_cast<int>(origin.y) + yDist(rng),
        static_cast<int>(origin.z) + xzDist(rng)
    );
}

glm::ivec3 findRandomTargetAway(
    const glm::vec3& origin,
    const glm::vec3& threat,
    int xzRange, int yRange,
    std::mt19937& rng)
{
    // Try 10 positions, pick one that moves away from threat
    glm::vec3 awayDir = glm::normalize(origin - threat);
    glm::ivec3 best = glm::ivec3(origin);
    float bestDot = -999.0f;

    for (int i = 0; i < 10; ++i) {
        glm::ivec3 candidate = findRandomTarget(origin, xzRange, yRange, rng);
        glm::vec3 dir = glm::vec3(candidate) - origin;
        float len = glm::length(dir);
        if (len < 0.01f) continue;
        float d = glm::dot(dir / len, awayDir);
        if (d > bestDot) {
            bestDot = d;
            best = candidate;
        }
    }
    return best;
}

// =====================================================================
// Shared helper: follow a path with velocity-based movement.
//
// Flat-ground optimization:
//   A* produces cardinal-only waypoints, which causes zig-zag movement.
//   When all remaining waypoints share the same Y, we skip them and
//   walk directly toward the final destination for natural diagonal lines.
//   Non-flat paths (steps/drops) still follow waypoints for safe footing.
//
// Movement direction also updates the mob's facing rotation.
// =====================================================================
static void followPath(
    entt::registry& reg, entt::entity entity,
    Path& path, float speed, float dt)
{
    if (path.isFinished()) return;

    auto* t = reg.try_get<ecs::components::Transform>(entity);
    auto* vel = reg.try_get<ecs::components::Velocity>(entity);
    auto* mobAI = reg.try_get<ecs::components::MobAI>(entity);
    if (!t || !vel) return;

    // Determine movement target.
    // On flat ground (all remaining waypoints at the same Y), walk directly to
    // the final destination — this gives natural diagonal movement instead of
    // following the cardinal-direction A* grid.  Fall back to waypoint-by-
    // waypoint following only when the path has Y changes (steps/drops).
    glm::vec3 target;
    {
        bool flatPath = true;
        int baseY = path.points[path.currentIndex].y;
        for (int i = path.currentIndex + 1; i < static_cast<int>(path.points.size()); ++i) {
            if (path.points[i].y != baseY) {
                flatPath = false;
                break;
            }
        }

        if (flatPath && path.remaining() > 1) {
            // Beeline to final destination
            auto& last = path.points.back();
            target = glm::vec3(last.x + 0.5f, static_cast<float>(last.y), last.z + 0.5f);
        } else {
            // Follow waypoints for non-flat terrain
            target = path.getCurrentPos();
        }
    }

    glm::vec3 diff = target - t->position;
    diff.y = 0.0f; // horizontal movement only, physics handles Y

    float dist = glm::length(diff);
    if (dist < 0.5f) {
        // Reached current target — if beelining we're done, otherwise advance
        path.advance();
        while (!path.isFinished()) {
            // Skip any waypoints we've already passed
            glm::vec3 wp = path.getCurrentPos();
            glm::vec3 d = wp - t->position;
            d.y = 0.0f;
            if (glm::length(d) < 0.5f) {
                path.advance();
            } else {
                break;
            }
        }
        if (path.isFinished()) {
            vel->linear.x = 0.0f;
            vel->linear.z = 0.0f;
            return;
        }
        target = path.getCurrentPos();
        diff = target - t->position;
        diff.y = 0.0f;
        dist = glm::length(diff);
    }

    if (dist > 0.01f) {
        glm::vec3 dir = diff / dist;
        // speed param is already in blocks/sec (mob moveSpeed * goal multiplier)
        vel->linear.x = dir.x * speed;
        vel->linear.z = dir.z * speed;

        // Update facing direction
        // Model faces -Z by default, so rotate -Z toward movement dir
        if (mobAI) {
            mobAI->targetYaw = std::atan2(-dir.x, -dir.z);
            mobAI->hasLookTarget = true;
        }

        // Rotate the entity to face movement direction
        float yaw = std::atan2(-dir.x, -dir.z);
        t->setRotation(glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
    }
}

// =====================================================================
// WanderGoal
// =====================================================================
WanderGoal::WanderGoal(entt::registry& reg, entt::entity entity,
                       const ChunkManager* cm,
                       const blockstate::BlockStateRegistry& bsr,
                       float speed, int cooldownTicks)
    : m_registry(reg), m_entity(entity), m_cm(cm), m_bsr(bsr),
      m_speed(speed), m_cooldownTicks(cooldownTicks),
      m_rng(std::random_device{}())
{
    setMutexBits(MUTEX_MOVE);
}

bool WanderGoal::shouldExecute() {
    // 1/cooldownTicks chance per check (checked every 3 ticks by scheduler)
    std::uniform_int_distribution<int> dist(0, m_cooldownTicks);
    if (dist(m_rng) != 0) return false;

    auto* t = m_registry.try_get<ecs::components::Transform>(m_entity);
    if (!t) return false;

    // Find a random nearby position
    glm::ivec3 target = findRandomTarget(t->position, 10, 3, m_rng);

    // Pathfind to it
    glm::ivec3 start(
        static_cast<int>(std::floor(t->position.x)),
        static_cast<int>(std::floor(t->position.y)),
        static_cast<int>(std::floor(t->position.z))
    );
    m_path = Pathfinder::findPath(m_cm, m_bsr, start, target, 0.6f, 1.8f, 16.0f);
    return !m_path.isFinished() && m_path.points.size() > 1;
}

bool WanderGoal::continueExecuting() {
    return !m_path.isFinished();
}

void WanderGoal::startExecuting() {
    // Path already computed in shouldExecute
}

void WanderGoal::updateTask(float dt) {
    followPath(m_registry, m_entity, m_path, m_speed, dt);
}

void WanderGoal::resetTask() {
    m_path = Path{};
    auto* vel = m_registry.try_get<ecs::components::Velocity>(m_entity);
    if (vel) {
        vel->linear.x = 0.0f;
        vel->linear.z = 0.0f;
    }
}

// =====================================================================
// PanicGoal
// =====================================================================
PanicGoal::PanicGoal(entt::registry& reg, entt::entity entity,
                     const ChunkManager* cm,
                     const blockstate::BlockStateRegistry& bsr,
                     float speed)
    : m_registry(reg), m_entity(entity), m_cm(cm), m_bsr(bsr),
      m_speed(speed), m_rng(std::random_device{}())
{
    setMutexBits(MUTEX_MOVE);
}

bool PanicGoal::shouldExecute() {
    auto* hurt = m_registry.try_get<ecs::components::HurtState>(m_entity);
    if (!hurt || !hurt->wasHurt) return false;

    auto* t = m_registry.try_get<ecs::components::Transform>(m_entity);
    if (!t) return false;

    // Find a position away from the attacker
    glm::vec3 threatPos = t->position;
    if (hurt->attacker != entt::null && m_registry.valid(hurt->attacker)) {
        auto* attackerT = m_registry.try_get<ecs::components::Transform>(hurt->attacker);
        if (attackerT) threatPos = attackerT->position;
    }

    glm::ivec3 target = findRandomTargetAway(t->position, threatPos, 5, 4, m_rng);
    glm::ivec3 start(
        static_cast<int>(std::floor(t->position.x)),
        static_cast<int>(std::floor(t->position.y)),
        static_cast<int>(std::floor(t->position.z))
    );
    m_path = Pathfinder::findPath(m_cm, m_bsr, start, target, 0.6f, 1.8f, 16.0f);
    return !m_path.isFinished() && m_path.points.size() > 1;
}

bool PanicGoal::continueExecuting() {
    return !m_path.isFinished();
}

void PanicGoal::startExecuting() {}

void PanicGoal::updateTask(float dt) {
    followPath(m_registry, m_entity, m_path, m_speed, dt);
}

void PanicGoal::resetTask() {
    m_path = Path{};
    auto* vel = m_registry.try_get<ecs::components::Velocity>(m_entity);
    if (vel) {
        vel->linear.x = 0.0f;
        vel->linear.z = 0.0f;
    }
    // Clear hurt state after fleeing
    auto* hurt = m_registry.try_get<ecs::components::HurtState>(m_entity);
    if (hurt) hurt->wasHurt = false;
}

// =====================================================================
// LookIdleGoal
// =====================================================================
LookIdleGoal::LookIdleGoal(entt::registry& reg, entt::entity entity)
    : m_registry(reg), m_entity(entity), m_rng(std::random_device{}())
{
    setMutexBits(MUTEX_MOVE_LOOK);
}

bool LookIdleGoal::shouldExecute() {
    std::uniform_int_distribution<int> dist(0, 49); // ~2% chance per check
    return dist(m_rng) == 0;
}

bool LookIdleGoal::continueExecuting() {
    return m_lookTimer > 0.0f;
}

void LookIdleGoal::startExecuting() {
    // Pick random yaw and duration
    std::uniform_real_distribution<float> yawDist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> timeDist(0.67f, 1.33f); // 20-40 ticks @ 30hz
    m_targetYaw = yawDist(m_rng);
    m_lookTimer = timeDist(m_rng);
}

void LookIdleGoal::updateTask(float dt) {
    m_lookTimer -= dt;

    auto* t = m_registry.try_get<ecs::components::Transform>(m_entity);
    if (!t) return;

    // Smoothly rotate toward target yaw
    glm::quat target = glm::angleAxis(m_targetYaw, glm::vec3(0, 1, 0));
    t->setRotation(glm::slerp(t->rotation, target, dt * 3.0f));
}

// =====================================================================
// MeleeAttackGoal
// =====================================================================
MeleeAttackGoal::MeleeAttackGoal(entt::registry& reg, entt::entity entity,
                                 const ChunkManager* cm,
                                 const blockstate::BlockStateRegistry& bsr,
                                 float speed, float attackDamage)
    : m_registry(reg), m_entity(entity), m_cm(cm), m_bsr(bsr),
      m_speed(speed), m_attackDamage(attackDamage)
{
    setMutexBits(MUTEX_MOVE_LOOK);
}

entt::entity MeleeAttackGoal::findTarget() const {
    auto* atk = m_registry.try_get<ecs::components::AttackTarget>(m_entity);
    if (!atk || atk->target == entt::null) return entt::null;
    if (!m_registry.valid(atk->target)) return entt::null;
    return atk->target;
}

bool MeleeAttackGoal::shouldExecute() {
    return findTarget() != entt::null;
}

bool MeleeAttackGoal::continueExecuting() {
    entt::entity target = findTarget();
    if (target == entt::null) return false;

    // Check distance — give up if target is > 32 blocks away
    auto* myT = m_registry.try_get<ecs::components::Transform>(m_entity);
    auto* targetT = m_registry.try_get<ecs::components::Transform>(target);
    if (!myT || !targetT) return false;

    float distSq = glm::dot(myT->position - targetT->position, myT->position - targetT->position);
    return distSq < 32.0f * 32.0f;
}

void MeleeAttackGoal::startExecuting() {
    m_attackCooldown = 0.0f;
    m_repathTimer = 0.0f;
}

void MeleeAttackGoal::updateTask(float dt) {
    entt::entity target = findTarget();
    if (target == entt::null) return;

    auto* myT = m_registry.try_get<ecs::components::Transform>(m_entity);
    auto* targetT = m_registry.try_get<ecs::components::Transform>(target);
    if (!myT || !targetT) return;

    glm::vec3 diff = targetT->position - myT->position;
    float distSq = glm::dot(diff, diff);

    // Look at target
    glm::vec3 horizDiff = diff;
    horizDiff.y = 0.0f;
    float horizLen = glm::length(horizDiff);
    if (horizLen > 0.01f) {
        float yaw = std::atan2(-horizDiff.x, -horizDiff.z);
        myT->setRotation(glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
    }

    // Re-path periodically
    m_repathTimer -= dt;
    if (m_repathTimer <= 0.0f) {
        m_repathTimer = 0.5f; // repath every 0.5s
        if (distSq > 1024.0f) m_repathTimer += 0.33f; // slower repath when far
        if (distSq > 256.0f) m_repathTimer += 0.17f;

        glm::ivec3 start(
            static_cast<int>(std::floor(myT->position.x)),
            static_cast<int>(std::floor(myT->position.y)),
            static_cast<int>(std::floor(myT->position.z))
        );
        glm::ivec3 goal(
            static_cast<int>(std::floor(targetT->position.x)),
            static_cast<int>(std::floor(targetT->position.y)),
            static_cast<int>(std::floor(targetT->position.z))
        );
        m_path = Pathfinder::findPath(m_cm, m_bsr, start, goal, 0.6f, 1.8f, 32.0f);
    }

    // Follow path
    followPath(m_registry, m_entity, m_path, m_speed, dt);

    // Check attack range (~2 block reach)
    m_attackCooldown -= dt;
    float attackRangeSq = 2.0f * 2.0f; // 2 blocks
    if (distSq <= attackRangeSq && m_attackCooldown <= 0.0f) {
        m_attackCooldown = 0.67f; // ~20 ticks at 30hz

        // Deal damage to target
        auto* targetHealth = m_registry.try_get<ecs::components::Health>(target);
        if (targetHealth) {
            targetHealth->hp -= static_cast<int>(m_attackDamage);

            // Mark target as hurt (for panic behavior)
            auto* targetHurt = m_registry.try_get<ecs::components::HurtState>(target);
            if (targetHurt) {
                targetHurt->wasHurt = true;
                targetHurt->hurtTimer = 0.0f;
                targetHurt->attacker = m_entity;
            }
        }
    }
}

void MeleeAttackGoal::resetTask() {
    m_path = Path{};
    auto* vel = m_registry.try_get<ecs::components::Velocity>(m_entity);
    if (vel) {
        vel->linear.x = 0.0f;
        vel->linear.z = 0.0f;
    }
}

// =====================================================================
// KeepDistanceGoal — Skeleton-style: strafe around target, maintain range.
//
// Distance zones (relative to m_preferredDist):
//   < 0.6x  : Way too close — heavy retreat + light strafe
//   < 0.85x : Slightly close — light retreat + heavy strafe
//   > 1.3x  : Too far — approach with light strafe
//   else    : Sweet spot — pure strafe (circle target)
//
// Strafe direction flips randomly every 2-4 seconds (30% chance).
// =====================================================================
KeepDistanceGoal::KeepDistanceGoal(entt::registry& reg, entt::entity entity,
                                   const ChunkManager* cm,
                                   const blockstate::BlockStateRegistry& bsr,
                                   float speed, float preferredDist, float maxDist)
    : m_registry(reg), m_entity(entity), m_cm(cm), m_bsr(bsr),
      m_speed(speed), m_preferredDist(preferredDist), m_maxDist(maxDist),
      m_rng(std::random_device{}())
{
    setMutexBits(MUTEX_MOVE_LOOK);
}

entt::entity KeepDistanceGoal::findTarget() const {
    auto* atk = m_registry.try_get<ecs::components::AttackTarget>(m_entity);
    if (!atk || atk->target == entt::null) return entt::null;
    if (!m_registry.valid(atk->target)) return entt::null;
    return atk->target;
}

bool KeepDistanceGoal::shouldExecute() {
    return findTarget() != entt::null;
}

bool KeepDistanceGoal::continueExecuting() {
    entt::entity target = findTarget();
    if (target == entt::null) return false;

    auto* myT = m_registry.try_get<ecs::components::Transform>(m_entity);
    auto* targetT = m_registry.try_get<ecs::components::Transform>(target);
    if (!myT || !targetT) return false;

    float distSq = glm::dot(myT->position - targetT->position, myT->position - targetT->position);
    return distSq < m_maxDist * m_maxDist;
}

void KeepDistanceGoal::startExecuting() {
    m_strafeTimer = 0.0f;
    // Random initial strafe direction
    std::uniform_int_distribution<int> coinFlip(0, 1);
    m_strafeDir = coinFlip(m_rng) ? 1 : -1;
}

void KeepDistanceGoal::updateTask(float dt) {
    entt::entity target = findTarget();
    if (target == entt::null) return;

    auto* myT = m_registry.try_get<ecs::components::Transform>(m_entity);
    auto* targetT = m_registry.try_get<ecs::components::Transform>(target);
    auto* vel = m_registry.try_get<ecs::components::Velocity>(m_entity);
    if (!myT || !targetT || !vel) return;

    glm::vec3 diff = targetT->position - myT->position;
    diff.y = 0.0f;
    float dist = glm::length(diff);

    // Always face the target
    if (dist > 0.01f) {
        glm::vec3 dir = diff / dist;
        float yaw = std::atan2(-dir.x, -dir.z);
        myT->setRotation(glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
    }

    // Change strafe direction periodically (every 2-4 seconds)
    m_strafeTimer -= dt;
    if (m_strafeTimer <= 0.0f) {
        std::uniform_real_distribution<float> timerDist(2.0f, 4.0f);
        m_strafeTimer = timerDist(m_rng);
        // 30% chance to flip direction
        std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
        if (chanceDist(m_rng) < 0.3f) {
            m_strafeDir = -m_strafeDir;
        }
    }

    // Movement logic: approach/retreat + strafe
    glm::vec3 moveDir(0.0f);

    if (dist < 0.1f) {
        // Too close, just back away
        moveDir = -glm::normalize(diff);
    } else {
        glm::vec3 toTarget = diff / dist;
        // Strafe perpendicular to target direction
        glm::vec3 strafeDir = glm::vec3(-toTarget.z, 0.0f, toTarget.x) * static_cast<float>(m_strafeDir);

        if (dist < m_preferredDist * 0.6f) {
            // Way too close — back away quickly
            moveDir = -toTarget * 0.7f + strafeDir * 0.3f;
        } else if (dist < m_preferredDist * 0.85f) {
            // A bit too close — back away slowly while strafing
            moveDir = -toTarget * 0.3f + strafeDir * 0.7f;
        } else if (dist > m_preferredDist * 1.3f) {
            // Too far — approach while strafing slightly
            moveDir = toTarget * 0.7f + strafeDir * 0.3f;
        } else {
            // In the sweet spot — pure strafe
            moveDir = strafeDir;
        }
    }

    float len = glm::length(moveDir);
    if (len > 0.01f) {
        moveDir /= len;

        // Edge avoidance: before applying velocity, check if moving in
        // this direction would step off solid ground.  Probe 1 block ahead
        // in the movement direction and verify there's a solid block below.
        // This prevents the skeleton from walking off platforms to maintain
        // distance 
        {
            glm::vec3 probe = myT->position + moveDir * 1.0f;
            int px = static_cast<int>(std::floor(probe.x));
            int py = static_cast<int>(std::floor(myT->position.y));
            int pz = static_cast<int>(std::floor(probe.z));

            bool groundAhead = Pathfinder::isSolid(m_cm, m_bsr, px, py - 1, pz);
            if (!groundAhead) {
                // No ground ahead — suppress movement to avoid walking off
                vel->linear.x = 0.0f;
                vel->linear.z = 0.0f;

                // Flip strafe direction so the mob tries the other way next tick
                m_strafeDir = -m_strafeDir;
                m_strafeTimer = 0.5f;
                return;
            }
        }

        vel->linear.x = moveDir.x * m_speed;
        vel->linear.z = moveDir.z * m_speed;
    } else {
        vel->linear.x = 0.0f;
        vel->linear.z = 0.0f;
    }
}

void KeepDistanceGoal::resetTask() {
    auto* vel = m_registry.try_get<ecs::components::Velocity>(m_entity);
    if (vel) {
        vel->linear.x = 0.0f;
        vel->linear.z = 0.0f;
    }
}

// =====================================================================
// FindTargetGoal
// =====================================================================
FindTargetGoal::FindTargetGoal(entt::registry& reg, entt::entity entity,
                               float range)
    : m_registry(reg), m_entity(entity), m_range(range)
{
    setMutexBits(MUTEX_NONE); // Target selection runs alongside everything
}

bool FindTargetGoal::shouldExecute() {
    auto* myT = m_registry.try_get<ecs::components::Transform>(m_entity);
    if (!myT) return false;

    // Find nearest player within range
    float bestDistSq = m_range * m_range;
    entt::entity bestTarget = entt::null;

    auto view = m_registry.view<ecs::components::PlayerTag, ecs::components::Transform>();
    for (auto [e, pt] : view.each()) {
        glm::vec3 diff = pt.position - myT->position;
        float distSq = glm::dot(diff, diff);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestTarget = e;
        }
    }

    if (bestTarget != entt::null) {
        // Set as attack target
        auto& atk = m_registry.get_or_emplace<ecs::components::AttackTarget>(m_entity);
        atk.target = bestTarget;
        return true;
    }
    return false;
}

bool FindTargetGoal::continueExecuting() {
    auto* atk = m_registry.try_get<ecs::components::AttackTarget>(m_entity);
    if (!atk || atk->target == entt::null) return false;
    if (!m_registry.valid(atk->target)) {
        atk->target = entt::null;
        return false;
    }

    // Check if target is still in range
    auto* myT = m_registry.try_get<ecs::components::Transform>(m_entity);
    auto* targetT = m_registry.try_get<ecs::components::Transform>(atk->target);
    if (!myT || !targetT) return false;

    float distSq = glm::dot(myT->position - targetT->position, myT->position - targetT->position);
    return distSq < (m_range * 1.5f) * (m_range * 1.5f); // Extended range before losing target
}

void FindTargetGoal::startExecuting() {}

void FindTargetGoal::resetTask() {
    auto* atk = m_registry.try_get<ecs::components::AttackTarget>(m_entity);
    if (atk) atk->target = entt::null;
}

} // namespace ai
