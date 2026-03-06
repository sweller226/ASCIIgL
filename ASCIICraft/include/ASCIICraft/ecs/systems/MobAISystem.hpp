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

namespace ecs::systems {

// =====================================================================
// MobAISystem — Drives mob AI brains, pathfinding, death/despawn
// =====================================================================
class MobAISystem {
public:
    explicit MobAISystem(entt::registry& registry);

    // Initialize AI brains for all existing mobs (call after spawning)
    void initializeMobs();

    // Initialize AI brain for a specific mob entity
    void initializeMob(entt::entity e);

    // Run one AI tick. Call every frame — AI goals internally throttle.
    // Update order: hurt timers → AI schedulers → push apart → death → despawn
    void update(float dt);

private:
    entt::registry& m_registry;

    // Helpers
    const ChunkManager* getChunkManager() const;

    void updateDeathAndDespawn(float dt);
    void updateHurtTimers(float dt);
    void despawnOutOfRange();
    void pushMobsApart();
};

} // namespace ecs::systems
