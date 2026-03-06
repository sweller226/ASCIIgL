#pragma once
// =========================================================================
// PlayerCombatSystem.hpp — Player melee attack on mobs
//
// When the player presses the attack key (F), this system finds the mob
// closest to the player's crosshair (within reach and a cone angle) and
// deals damage to it.  Uses the same eye-position and look-direction as
// the block-breaking raycast.
//
// MC-style: 3 blocks reach, 1 base damage (bare fist), 0.5s cooldown.
// =========================================================================

#include <ASCIICraft/events/EventBus.hpp>
#include <entt/entt.hpp>

namespace ecs::systems {

class PlayerCombatSystem {
public:
    PlayerCombatSystem(entt::registry& registry, EventBus& eventBus);

    /// Process attack events. Call once per frame in Game::Update().
    void Update();

private:
    entt::registry& m_registry;
    EventBus& m_eventBus;

    float m_attackCooldown = 0.0f;          // seconds remaining before next attack
    static constexpr float ATTACK_COOLDOWN_TIME = 0.5f;  // MC ~10 ticks
    static constexpr float ATTACK_REACH = 3.0f;          // blocks
    static constexpr float ATTACK_DAMAGE = 1.0f;         // bare fist: 1 HP per hit (vanilla MC)
    static constexpr float AIM_CONE_DOT = 0.9f;          // ~25° half-angle

    /// Find the best mob target along the player's look direction and deal damage.
    void TryAttack();
};

} // namespace ecs::systems
