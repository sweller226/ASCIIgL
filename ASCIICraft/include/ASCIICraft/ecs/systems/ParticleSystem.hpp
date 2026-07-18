#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <ASCIIgL/util/EventBus.hpp>

#include <ASCIICraft/events/ParticleSpawnEvent.hpp>
#include <ASCIICraft/events/BlockHitEvent.hpp>

#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <ASCIICraft/util/RNG.hpp>

namespace ASCIIgL { class EventBus; }
namespace events { struct BreakBlockEvent; }

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

    // -------------------------------------------------------------------------
    // Block break / hit particles
    // -------------------------------------------------------------------------
    static constexpr int   BREAK_BURST_COUNT   = 18;
    static constexpr int   BREAK_MESH_VARIANTS = 4;    // sub-UV variants shared per burst
    static constexpr float BREAK_LIFETIME      = 0.6f;
    static constexpr float BREAK_SCALE         = 0.06f; // unit quad spans 2 units
    static constexpr int   HIT_PUFF_COUNT      = 2;
    static constexpr float HIT_LIFETIME        = 0.4f;
    static constexpr float HIT_SCALE           = 0.045f;

    void EmitAmbientLeafParticles(float dt, components::Transform& t);
    void ProcessSpawnEvents();
    void ProcessBlockBreakEvents();
    void ProcessBlockHitEvents();
    void DespawnDeadParticles();
    bool InitLeafMaterial();

    /// Texture-array layer of the given blockstate's model (first face), or -1.
    float ResolveBlockTextureLayer(uint32_t stateId) const;
    /// Billboard quad sampling a random 4x4 sub-region of the given terrain layer.
    std::shared_ptr<ASCIIgL::Mesh> BuildBlockParticleMesh(float textureLayer);
    /// Terrain-array material used for block particles (lazily fetched).
    std::shared_ptr<ASCIIgL::Material> GetBlockParticleMaterial();

    entt::registry&    m_registry;
    ASCIIgL::EventBus& eventBus;

    std::shared_ptr<ASCIIgL::Mesh>     m_leafMesh;
    std::shared_ptr<ASCIIgL::Material> m_leafMaterial;
    std::shared_ptr<ASCIIgL::Material> m_blockParticleMaterial;

    int   m_particleCount = 0;
    util::RNG m_rng;
};

} // namespace ecs::systems