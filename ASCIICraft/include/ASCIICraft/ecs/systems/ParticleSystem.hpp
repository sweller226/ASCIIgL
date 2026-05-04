#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <ASCIIgL/util/EventBus.hpp>

#include <ASCIICraft/events/ParticleSpawnEvent.hpp>

#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <ASCIICraft/util/RNG.hpp>

namespace ASCIIgL { class EventBus; }

namespace ecs::systems {

class ParticleSystem : public ISystem {
public:
    ParticleSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);

    void Update();
    bool Init();

private:
    // ALL of these constants are for the random ambient particles, this shouldn't be in ParticleSystem
    // but will be for now until I get more particles and need to modularize this.
    // -------------------------------------------------------------------------
    // Spawn volume
    // -------------------------------------------------------------------------
    static constexpr int   MAX_PARTICLES     = 70;
    static constexpr int   SPAWN_PER_FRAME   = 3;
    static constexpr float SPAWN_RADIUS      = 32.0f;  // outer XZ half-extent
    static constexpr float SPAWN_HEIGHT      = 16.0f;  // outer Y half-extent
    static constexpr float EXCLUSION_RADIUS  = 5.0f;   // inner XZ half-extent
    static constexpr float EXCLUSION_HEIGHT  = 3.0f;   // inner Y half-extent
    static constexpr float MAX_SPAWN_HEIGHT    = 90.0f;
    static constexpr float MIN_SPAWN_HEIGHT    = 77.0f;

    // -------------------------------------------------------------------------
    // Particle behaviour
    // -------------------------------------------------------------------------
    static constexpr float LIFETIME         = 8.0f;
    static constexpr float LIFETIME_JITTER  = 0.3f;   // ± fraction of LIFETIME
    static constexpr float DRIFT_SPEED      = 1.5f;
    static constexpr float FALL_SPEED       = 3.0f;
    static constexpr float MIN_SPEED        = 1.8f;
    static constexpr float DAMPING          = 0.4f;
    static constexpr float PARTICLE_SCALE   = 0.125f;

    void EmitAmbientLeafParticles(float dt, components::Transform& t);
    void ProcessSpawnEvents();
    void DespawnDeadParticles();
    bool InitLeafMaterial();

    entt::registry&    m_registry;
    ASCIIgL::EventBus& eventBus;

    std::shared_ptr<ASCIIgL::Mesh>     m_leafMesh;
    std::shared_ptr<ASCIIgL::Material> m_leafMaterial;

    int   m_particleCount = 0;
    util::RNG m_rng;
};

} // namespace ecs::systems