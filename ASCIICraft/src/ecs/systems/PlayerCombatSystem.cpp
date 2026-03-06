// =========================================================================
// PlayerCombatSystem.cpp — Player melee attack implementation
//
// Hit detection approach:
//   1. Get the player's eye position and look direction (same as MiningSystem)
//   2. For every mob within ATTACK_REACH distance, check if the mob's center
//      is within a narrow cone (AIM_CONE_DOT) of the look direction
//   3. Pick the closest valid target and deal ATTACK_DAMAGE to its Health
//   4. Set the mob's HurtState so PanicGoal (passive) or retaliation can fire
//   5. Enforce a cooldown between swings
//
// This is intentionally simple — no weapon items, no knockback, no crit.
// =========================================================================

#include <ASCIICraft/ecs/systems/PlayerCombatSystem.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/MobComponents.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIIgL/engine/FPSClock.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <glm/glm.hpp>
#include <cmath>

namespace ecs::systems {

PlayerCombatSystem::PlayerCombatSystem(entt::registry& registry, EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{}

void PlayerCombatSystem::Update() {
    // Tick down cooldown
    float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();
    if (m_attackCooldown > 0.0f) {
        m_attackCooldown -= dt;
    }

    // Check for attack input
    for ([[maybe_unused]] const auto& e : m_eventBus.view<events::AttackActionPressedEvent>()) {
        TryAttack();
        break; // consume only one event per frame
    }
}

void PlayerCombatSystem::TryAttack() {
    // Cooldown check
    if (m_attackCooldown > 0.0f) return;

    // Find the player entity
    entt::entity playerEnt = components::GetPlayerEntity(m_registry);
    if (playerEnt == entt::null) return;

    // Get player eye position and look direction
    auto* head = m_registry.try_get<components::Head>(playerEnt);
    auto [position, success] = components::GetPos(playerEnt, m_registry);
    if (!head || !success) return;

    glm::vec3 eyePos = position + head->relativePos;
    glm::vec3 lookDir = glm::normalize(head->lookDir);

    // Find the closest mob within reach and aim cone
    float bestDistSq = ATTACK_REACH * ATTACK_REACH;
    entt::entity bestTarget = entt::null;

    auto view = m_registry.view<components::MobTag, components::Transform>();
    for (auto [e, mob, t] : view.each()) {
        // Skip dead mobs
        auto* death = m_registry.try_get<components::DeathState>(e);
        if (death && death->isDead) continue;

        // Compute the mob's center position (position + collider offset for height)
        glm::vec3 mobCenter = t.position;
        auto* col = m_registry.try_get<components::Collider>(e);
        if (col) {
            mobCenter += col->localOffset; // center of the AABB
        }

        glm::vec3 toMob = mobCenter - eyePos;
        float distSq = glm::dot(toMob, toMob);

        // Must be within reach
        if (distSq > ATTACK_REACH * ATTACK_REACH) continue;
        if (distSq < 0.001f) continue; // don't hit self-overlap

        // Must be within aim cone
        glm::vec3 dirToMob = glm::normalize(toMob);
        float dot = glm::dot(dirToMob, lookDir);
        if (dot < AIM_CONE_DOT) continue;

        // Closest wins
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestTarget = e;
        }
    }

    if (bestTarget == entt::null) return;

    // Deal damage
    auto* health = m_registry.try_get<components::Health>(bestTarget);
    if (health) {
        health->hp -= static_cast<int>(ATTACK_DAMAGE);
        ASCIIgL::Logger::Info("Player attacked mob (type "
            + std::to_string(m_registry.get<components::MobTag>(bestTarget).typeId)
            + ") for " + std::to_string(static_cast<int>(ATTACK_DAMAGE))
            + " damage. HP remaining: " + std::to_string(health->hp));
    }

    // Mark mob as hurt (triggers PanicGoal on passive mobs)
    auto* hurt = m_registry.try_get<components::HurtState>(bestTarget);
    if (hurt) {
        hurt->wasHurt = true;
        hurt->hurtTimer = 0.0f;
        hurt->attacker = playerEnt;
    }

    // Start cooldown
    m_attackCooldown = ATTACK_COOLDOWN_TIME;
}

} // namespace ecs::systems
