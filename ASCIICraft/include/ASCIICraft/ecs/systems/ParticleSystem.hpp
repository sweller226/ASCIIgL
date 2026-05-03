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

/// Manages ambient particle generation and consumes ParticleSpawnEvents to
/// create particle entities in the registry.
class ParticleSystem : public ISystem {
public:
    ParticleSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);

    /// Call once per frame. Emits ambient particle events and processes the spawn queue.
    void Update();

    bool Init();

private:
    /// Walks the registry to emit ambient particle events (e.g. torches, water).
    void EmitAmbientLeafParticles(float dt, components::Transform &t);

    /// Consumes all pending ParticleSpawnEvents and creates particle entities.
    void ProcessSpawnEvents();

    /// Destroys all particle entities flagged for despawn.
    void DespawnDeadParticles();

    
    entt::registry &m_registry;
    ASCIIgL::EventBus &eventBus;
    
    // temp vars until I make this system more modular
    float m_ambientTimer = 0.0f;

    bool InitLeafMaterial();
    std::shared_ptr<ASCIIgL::Mesh>     m_leafMesh;
    std::shared_ptr<ASCIIgL::Material> m_leafMaterial;

    util::RNG m_rng;
};

} // namespace ecs::systems