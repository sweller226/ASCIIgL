#include <ASCIICraft/ecs/systems/ParticleSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/ecs/components/Lifetime.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>

#include <ASCIICraft/util/QuadMeshBuilder.hpp>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Shader.hpp>

#include <ASCIICraft/rendering/LeafParticleShaders.hpp>

namespace ecs::systems {
    ParticleSystem::ParticleSystem(entt::registry &registry, ASCIIgL::EventBus& eventBus) 
        : m_registry(registry)
        , eventBus(eventBus) {}

    void ParticleSystem::Update() {
        entt::entity p_ent = components::GetPlayerEntity(m_registry);
        if (p_ent == entt::null || !m_registry.valid(p_ent)) return;

        const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

        auto& t = m_registry.get<components::Transform>(p_ent);

        // EmitAmbientLeafParticles(dt, t);
        ProcessSpawnEvents();
        DespawnDeadParticles();
    }

    void ParticleSystem::DespawnDeadParticles() {
        entt::entity p_ent = components::GetPlayerEntity(m_registry);
        const glm::vec3 playerPos = (p_ent != entt::null && m_registry.valid(p_ent))
            ? m_registry.get<components::Transform>(p_ent).position
            : glm::vec3(0.0f);

        constexpr float despawnRadius2 = SPAWN_RADIUS * SPAWN_RADIUS;

        auto view = m_registry.view<components::Velocity, components::Lifetime, components::Transform>();

        for (auto [ent, velocity, lifetime, t] : view.each()) {
            const glm::vec3 delta = t.position - playerPos;
            const bool tooFar = glm::dot(delta, delta) > despawnRadius2;

            if (lifetime.shouldDespawn || tooFar) {
                m_registry.destroy(ent);
                --m_particleCount;
            }
        }
    }

    void ParticleSystem::EmitAmbientLeafParticles(float dt, components::Transform& playerTransform) {
        if (m_particleCount >= MAX_PARTICLES) return;

        int toSpawn = std::min(SPAWN_PER_FRAME, MAX_PARTICLES - m_particleCount);

        for (int i = 0; i < toSpawn; ++i) {
            glm::vec3 offset;
            do {
                offset.x = (m_rng.NextFloat() - 0.5f) * 2.0f * SPAWN_RADIUS;
                offset.y = (m_rng.NextFloat() - 0.5f) * SPAWN_HEIGHT;
                offset.z = (m_rng.NextFloat() - 0.5f) * 2.0f * SPAWN_RADIUS;
            } while (
                std::abs(offset.x) < EXCLUSION_RADIUS &&
                std::abs(offset.y) < EXCLUSION_HEIGHT &&
                std::abs(offset.z) < EXCLUSION_RADIUS
            );

            glm::vec3 origin = playerTransform.position + offset;
            origin.y = std::clamp(origin.y, MIN_SPAWN_HEIGHT, MAX_SPAWN_HEIGHT);

            glm::vec3 velocity = {
                (m_rng.NextFloat() - 0.5f) * DRIFT_SPEED,
                (m_rng.NextFloat() - 0.5f) * FALL_SPEED,
                (m_rng.NextFloat() - 0.5f) * DRIFT_SPEED
            };

            float speed = glm::length(velocity);
            if (speed < MIN_SPEED && speed > 1e-6f)
                velocity = (velocity / speed) * MIN_SPEED;
            else if (speed <= 1e-6f)
                velocity = glm::vec3(0.0f, -MIN_SPEED, 0.0f);

            events::ParticleSpawnEvent event;
            event.origin   = origin;
            event.velocity = velocity;
            event.damping  = DAMPING;
            event.lifetime = LIFETIME * (1.0f - LIFETIME_JITTER + m_rng.NextFloat() * LIFETIME_JITTER);
            event.count    = 1;
            event.mesh     = m_leafMesh;
            event.material = m_leafMaterial;

            eventBus.emit(event);
        }
    }

    void ParticleSystem::ProcessSpawnEvents() {
        auto& events = eventBus.view<events::ParticleSpawnEvent>();

        for (const auto& e : events) {
            if (!e.mesh) continue;

            for (int i = 0; i < e.count; ++i) {
                entt::entity entity = m_registry.create();

                auto& t = m_registry.emplace<components::Transform>(entity);
                t.setPosition(e.origin);
                t.setScale(e.scale);

                auto& vel = m_registry.emplace<components::Velocity>(entity);
                vel.linear  = e.velocity;
                vel.damping = e.damping;

                auto& lifetime = m_registry.emplace<components::Lifetime>(entity);
                lifetime.maxLifetimeSeconds = e.lifetime;

                auto& renderable = m_registry.emplace<components::Renderable>(entity);
                renderable.renderType      = components::RenderType::ELEM_3D;
                renderable.mesh            = e.mesh;
                renderable.material        = e.material;
                renderable.visible         = true;
                renderable.backfaceCulling = false;
                renderable.transparent     = true;
                renderable.billboard       = true;

                ++m_particleCount;
            }
        }
    }

    bool ParticleSystem::InitLeafMaterial() {
        auto leafVS = ASCIIgL::Shader::CreateFromSource(LeafParticleShaders::GetVSSource(), ASCIIgL::ShaderType::Vertex);
        auto leafPS = ASCIIgL::Shader::CreateFromSource(LeafParticleShaders::GetPSSource(), ASCIIgL::ShaderType::Pixel);

        if (!leafVS || !leafVS->IsValid()) {
            ASCIIgL::Logger::Error("Failed to compile leaf particle vertex shader: " + leafVS->GetCompileError());
            return false;
        }
        if (!leafPS || !leafPS->IsValid()) {
            ASCIIgL::Logger::Error("Failed to compile leaf particle pixel shader: " + leafPS->GetCompileError());
            return false;
        }

        auto program = ASCIIgL::ShaderProgram::Create(
            std::move(leafVS), std::move(leafPS),
            ASCIIgL::VertFormats::PosColor(),
            LeafParticleShaders::GetUniformLayout()
        );
        if (!program || !program->IsValid()) {
            ASCIIgL::Logger::Error("Failed to create leaf particle shader program");
            return false;
        }

        auto material = ASCIIgL::Material::Create(std::move(program));
        if (!material) {
            ASCIIgL::Logger::Error("Failed to create leaf particle material");
            return false;
        }

        ASCIIgL::MaterialLibrary::GetInst().Register("leafParticleMaterial", std::move(material));
        return true;
    }

    bool ParticleSystem::Init() {
        if (!InitLeafMaterial()) return false;

        m_leafMesh = util::QuadMeshBuilder::BuildPosColorQuad(
            ASCIIgL::PaletteUtil::sRGB255ToLinear1(glm::ivec4(34, 139, 34, 255))
        );

        m_leafMaterial = ASCIIgL::MaterialLibrary::GetInst().Get("leafParticleMaterial");
        return true;
    }
}