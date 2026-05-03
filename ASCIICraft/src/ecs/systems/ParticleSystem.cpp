#include <ASCIICraft/ecs/systems/ParticleSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/ecs/components/Lifetime.hpp>
#include <ASCIICraft/ecs/components/ParticleMovement.hpp>
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

        EmitAmbientLeafParticles(dt, t);

        // Update opacity override for all particles based on remaining lifetime
        auto particleMat = ASCIIgL::MaterialLibrary::GetInst().Get("leafParticleMaterial");
        const ASCIIgL::UniformDescriptor* opacityDesc = particleMat
            ? particleMat->GetUniformDescriptor("opacityLevel")
            : nullptr;

        auto view = m_registry.view<components::ParticleMovement, components::Lifetime, components::Renderable>();
        for (auto [ent, movement, lifetime, renderable] : view.each()) {
            float opacity = (lifetime.maxLifetimeSeconds > 0.0f)
                ? 1.0f - (lifetime.ageSeconds / lifetime.maxLifetimeSeconds)
                : 1.0f;
            opacity = std::clamp(opacity, 0.0f, 1.0f);

            renderable.overrides.clear();

            if (opacityDesc) {
                ASCIIgL::Renderer::UniformOverride ov;
                ov.desc  = opacityDesc;
                ov.value = ASCIIgL::UniformValue(opacity);
                renderable.overrides.push_back(std::move(ov));
            }
        }

        DespawnDeadParticles();
    }

    void ParticleSystem::DespawnDeadParticles() {
        auto view = m_registry.view<components::ParticleMovement, components::Lifetime>();

        for (auto [ent, movement, lifetime] : view.each()) {
            if (lifetime.shouldDespawn)
                m_registry.destroy(ent);
        }
    }

    void ParticleSystem::EmitAmbientLeafParticles(float dt, components::Transform& playerTransform) {
        constexpr float spawnInterval  = 0.2f;  // seconds between spawns
        constexpr float spawnRadius    = 16.0f; // horizontal radius around player
        constexpr float spawnHeight    = 6.0f;  // max height above player
        constexpr float leafLifetime   = 4.0f;
        constexpr float driftSpeed     = 0.4f;  // horizontal float speed
        constexpr float fallSpeed      = 0.5f;  // downward drift speed

        m_ambientTimer += dt;
        if (m_ambientTimer < spawnInterval) return;
        m_ambientTimer = 0.0f;

        // Random position in a disc around the player, above them
        const float angle  = m_rng.NextFloat() * glm::two_pi<float>();
        const float radius = m_rng.NextFloat() * spawnRadius;

        glm::vec3 origin = playerTransform.position;
        origin.x += std::cos(angle) * radius;
        origin.z += std::sin(angle) * radius;
        origin.y += 2.0f + m_rng.NextFloat() * spawnHeight;

        // Gentle random horizontal drift + slow fall
        glm::vec3 velocity = {
            (m_rng.NextFloat() - 0.5f) * driftSpeed,
            -fallSpeed * (0.5f + m_rng.NextFloat() * 0.5f),
            (m_rng.NextFloat() - 0.5f) * driftSpeed
        };

        events::ParticleSpawnEvent event;
        event.origin       = origin;
        event.velocity     = velocity;
        event.acceleration = { 0.0f, -0.5f, 0.0f }; // light gravity
        event.drag         = 0.3f;                    // air resistance for floaty feel
        event.lifetime     = leafLifetime * (0.7f + m_rng.NextFloat() * 0.3f);
        event.count        = 1;
        event.mesh         = m_leafMesh;
        event.material     = m_leafMaterial;

        eventBus.emit(event);
    }

    void ParticleSystem::ProcessSpawnEvents() {
        auto& events = eventBus.view<events::ParticleSpawnEvent>();

        for (const auto& e : events) {
            if (!e.mesh) continue;

            for (int i = 0; i < e.count; ++i) {
                entt::entity entity = m_registry.create();

                auto& t = m_registry.emplace<components::Transform>(entity);
                t.setPosition(e.origin);
                t.setScale(glm::vec3(1, 1, 1));

                auto& movement = m_registry.emplace<components::ParticleMovement>(entity);
                movement.velocity     = e.velocity;
                movement.acceleration = e.acceleration;
                movement.drag         = e.drag;

                auto& lifetime = m_registry.emplace<components::Lifetime>(entity);
                lifetime.maxLifetimeSeconds = e.lifetime;

                auto& renderable = m_registry.emplace<components::Renderable>(entity);
                renderable.renderType = components::RenderType::ELEM_3D;
                renderable.mesh       = e.mesh;
                renderable.material   = e.material;
                renderable.visible    = true;
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
        InitLeafMaterial();

        m_leafMesh = util::QuadMeshBuilder::BuildPosColorQuad(
            ASCIIgL::PaletteUtil::sRGB255ToLinear1(glm::ivec4(34, 139, 34, 255))
        );

        m_leafMaterial = ASCIIgL::MaterialLibrary::GetInst().Get("leafParticleMaterial");
    }
}