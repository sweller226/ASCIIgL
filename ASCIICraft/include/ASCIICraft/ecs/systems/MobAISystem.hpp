#pragma once
// =========================================================================
// MobAISystem.hpp — Top-level system that drives all mob AI
//
// Responsibilities:
//   - Initialize AI brains (goal schedulers) per mob type
//   - Tick all AI schedulers each frame
//   - Entity-entity push to prevent mob stacking
//   - Death animation and despawn (HP <= 0)
//   - Render-distance despawn (too far from player)
//
// Mob type IDs (from MobFactory):
//   1=Pig, 2=Chicken, 3=Cow, 4=Creeper, 5=Sheep,
//   6=Skeleton, 7=Spider, 8=Zombie, 9=Ocelot, 10=Siamese, 11=Black Cat
// =========================================================================

#include <entt/entt.hpp>

class ChunkManager;
namespace ecs::factories { class MobFactory; }

namespace ecs::systems {

// =====================================================================
// MobAISystem — Drives mob AI brains, pathfinding, death/despawn
// =====================================================================
class MobAISystem {
public:
    // m_mobFactory is used to route despawns through MobFactory so m_active
    // stays consistent. Pass nullptr only in unit-test contexts.
    explicit MobAISystem(entt::registry& registry,
                         ecs::factories::MobFactory* mobFactory = nullptr);

    // Initialize AI brains for all existing mobs (call after spawning)
    void initializeMobs();

    // Initialize AI brain for a specific mob entity
    void initializeMob(entt::entity e);

    // Run one AI tick. Call every frame — AI goals internally throttle.
    // Update order: hurt timers → AI schedulers → push apart → death → despawn
    void update(float dt);

private:
    entt::registry& m_registry;
    ecs::factories::MobFactory* m_mobFactory;

    // Helpers
    const ChunkManager* getChunkManager() const;

    void updateDeathAndDespawn(float dt);
    void updateFallDamage(float dt);
    void updateHurtTimers(float dt);
    void despawnOutOfRange();
    void pushMobsApart();
    // Safely destroy a mob entity — routes through MobFactory when available
    // so m_active stays consistent.
    void destroyMob(entt::entity e);
};

} // namespace ecs::systems
