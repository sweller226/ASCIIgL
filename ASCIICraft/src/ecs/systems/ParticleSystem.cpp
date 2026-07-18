#include <ASCIICraft/ecs/systems/ParticleSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/ecs/components/Lifetime.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/ParticleTag.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>

#include <ASCIICraft/events/BreakBlockEvent.hpp>

#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

#include <ASCIICraft/util/MeshBuilderUtil.hpp>
#include <ASCIICraft/util/QuadMeshBuilder.hpp>

#include <ASCIIgL/engine/TextureLibrary.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/HLSLIncludes.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <ASCIICraft/rendering/LeafParticleShaders.hpp>

#include <array>
#include <cstring>

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
        ProcessBlockBreakEvents();
        ProcessBlockHitEvents();
        ProcessSpawnEvents();
        DespawnDeadParticles();
    }

    void ParticleSystem::DespawnDeadParticles() {
        entt::entity p_ent = components::GetPlayerEntity(m_registry);
        const glm::vec3 playerPos = (p_ent != entt::null && m_registry.valid(p_ent))
            ? m_registry.get<components::Transform>(p_ent).position
            : glm::vec3(0.0f);

        constexpr float despawnRadius2 = SPAWN_RADIUS * SPAWN_RADIUS;

        auto view = m_registry.view<components::ParticleTag, components::Lifetime, components::Transform>();

        for (auto ent : view) {
            auto& lifetime = view.get<components::Lifetime>(ent);
            auto& t = view.get<components::Transform>(ent);
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
                m_registry.emplace<components::ParticleTag>(entity);

                auto& t = m_registry.emplace<components::Transform>(entity);
                t.setPosition(e.origin);
                t.setScale(e.scale);

                auto& vel = m_registry.emplace<components::Velocity>(entity);
                vel.linear  = e.velocity;
                vel.damping = e.damping;

                if (e.gravity) {
                    m_registry.emplace<components::Gravity>(entity);
                }
                if (e.collideWorld) {
                    auto& col = m_registry.emplace<components::Collider>(entity);
                    col.halfExtents = glm::vec3(0.05f);
                    m_registry.emplace<components::GroundPhysics>(entity);
                }

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

    float ParticleSystem::ResolveBlockTextureLayer(uint32_t stateId) const {
        const auto* modelLibrary = m_registry.ctx().find<blockmodels::BlockModelLibrary>();
        if (!modelLibrary) return -1.0f;

        const blockstate::BlockModel* model = modelLibrary->GetModel(stateId);
        if (!model) return -1.0f;

        using V = ASCIIgL::VertStructs::PosUVLayer;
        const blockstate::RenderLayer* layer = nullptr;
        if (model->opaque.vertices.size() >= sizeof(V)) {
            layer = &model->opaque;
        } else if (model->transparent.vertices.size() >= sizeof(V)) {
            layer = &model->transparent;
        }
        if (!layer) return -1.0f;

        V firstVert{};
        std::memcpy(&firstVert, layer->vertices.data(), sizeof(V));
        return firstVert.Layer();
    }

    std::shared_ptr<ASCIIgL::Mesh> ParticleSystem::BuildBlockParticleMesh(float textureLayer) {
        using V = ASCIIgL::VertStructs::PosUVLayer;

        // Random 4x4 sub-region of the 16x16 block texture (like vanilla).
        const float u0 = m_rng.NextFloat() * 0.75f;
        const float v0 = m_rng.NextFloat() * 0.75f;
        const float u1 = u0 + 0.25f;
        const float v1 = v0 + 0.25f;

        const std::array<glm::vec3, 4> positions = {{
            {-1.0f, -1.0f, 0.0f},
            {-1.0f,  1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
        }};
        const std::array<glm::vec2, 4> uvs = {{
            {u0, v1},
            {u0, v0},
            {u1, v0},
            {u1, v1},
        }};

        std::vector<V> vertices;
        std::vector<int> indices;
        util::AppendQuadPosUVLayer(vertices, indices, positions, uvs, textureLayer);

        auto terrainTextureArray =
            ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");

        return std::make_shared<ASCIIgL::Mesh>(
            util::PackVerts(vertices),
            ASCIIgL::VertFormats::PosUVLayer(),
            std::move(indices),
            terrainTextureArray ? terrainTextureArray.get() : nullptr
        );
    }

    std::shared_ptr<ASCIIgL::Material> ParticleSystem::GetBlockParticleMaterial() {
        if (!m_blockParticleMaterial) {
            // PosUVLayer + terrain array + fog, with per-draw mvp/worldPos overrides
            // (fog params are synced each frame by EntityRenderSystem).
            m_blockParticleMaterial =
                ASCIIgL::MaterialLibrary::GetInst().Get("droppedItemBlockMaterial");
        }
        return m_blockParticleMaterial;
    }

    void ParticleSystem::ProcessBlockBreakEvents() {
        const auto& breakEvents = eventBus.view<events::BreakBlockEvent>();
        if (breakEvents.empty()) return;

        auto material = GetBlockParticleMaterial();
        if (!material) return;

        for (const auto& e : breakEvents) {
            const float textureLayer = ResolveBlockTextureLayer(e.stateId);
            if (textureLayer < 0.0f) continue;

            std::array<std::shared_ptr<ASCIIgL::Mesh>, BREAK_MESH_VARIANTS> meshes;
            for (auto& mesh : meshes) {
                mesh = BuildBlockParticleMesh(textureLayer);
            }

            const glm::vec3 blockMin(
                static_cast<float>(e.position.x),
                static_cast<float>(e.position.y),
                static_cast<float>(e.position.z)
            );
            const glm::vec3 center = blockMin + glm::vec3(0.5f);

            for (int i = 0; i < BREAK_BURST_COUNT; ++i) {
                const glm::vec3 offset(
                    0.1f + m_rng.NextFloat() * 0.8f,
                    0.1f + m_rng.NextFloat() * 0.8f,
                    0.1f + m_rng.NextFloat() * 0.8f
                );
                const glm::vec3 origin = blockMin + offset;

                glm::vec3 dir = origin - center;
                const float len = glm::length(dir);
                dir = (len > 1e-5f) ? dir / len : glm::vec3(0.0f, 1.0f, 0.0f);

                events::ParticleSpawnEvent spawn;
                spawn.origin = origin;
                spawn.velocity = dir * (1.0f + m_rng.NextFloat() * 1.5f)
                               + glm::vec3(0.0f, 1.5f + m_rng.NextFloat(), 0.0f);
                spawn.damping = 1.0f;
                spawn.lifetime = BREAK_LIFETIME * (0.7f + 0.6f * m_rng.NextFloat());
                spawn.count = 1;
                spawn.scale = glm::vec3(BREAK_SCALE * (0.8f + 0.4f * m_rng.NextFloat()));
                spawn.gravity = true;
                spawn.collideWorld = true;
                spawn.mesh = meshes[i % BREAK_MESH_VARIANTS];
                spawn.material = material;
                eventBus.emit(spawn);
            }
        }
    }

    void ParticleSystem::ProcessBlockHitEvents() {
        const auto& hitEvents = eventBus.view<events::BlockHitEvent>();
        if (hitEvents.empty()) return;

        auto material = GetBlockParticleMaterial();
        if (!material) return;

        for (const auto& e : hitEvents) {
            const float textureLayer = ResolveBlockTextureLayer(e.stateId);
            if (textureLayer < 0.0f) continue;

            const glm::vec3 center(
                static_cast<float>(e.position.x) + 0.5f,
                static_cast<float>(e.position.y) + 0.5f,
                static_cast<float>(e.position.z) + 0.5f
            );
            const glm::vec3 normal = FaceDirOutwardNormal(e.face);
            // Tangent basis for jittering across the face plane.
            const glm::vec3 tangentA = (std::abs(normal.y) > 0.5f)
                ? glm::vec3(1.0f, 0.0f, 0.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 tangentB = glm::cross(normal, tangentA);

            for (int i = 0; i < HIT_PUFF_COUNT; ++i) {
                const float ja = (m_rng.NextFloat() - 0.5f) * 0.8f;
                const float jb = (m_rng.NextFloat() - 0.5f) * 0.8f;
                const glm::vec3 origin = center + normal * 0.56f + tangentA * ja + tangentB * jb;

                events::ParticleSpawnEvent spawn;
                spawn.origin = origin;
                spawn.velocity = normal * (0.5f + m_rng.NextFloat() * 0.5f)
                               + glm::vec3(0.0f, 0.5f, 0.0f);
                spawn.damping = 1.0f;
                spawn.lifetime = HIT_LIFETIME * (0.7f + 0.6f * m_rng.NextFloat());
                spawn.count = 1;
                spawn.scale = glm::vec3(HIT_SCALE * (0.8f + 0.4f * m_rng.NextFloat()));
                spawn.gravity = true;
                spawn.collideWorld = true;
                spawn.mesh = BuildBlockParticleMesh(textureLayer);
                spawn.material = material;
                eventBus.emit(spawn);
            }
        }
    }

    bool ParticleSystem::InitLeafMaterial() {
        auto leafVS = ASCIIgL::Shader::CreateFromSource(LeafParticleShaders::GetVSSource(), ASCIIgL::ShaderType::Vertex);

        ASCIIgL::ShaderIncludeMap includes;
        ASCIIgL::HLSLIncludes::AddToMap(includes);
        auto leafPS = ASCIIgL::Shader::CreateFromSource(
            LeafParticleShaders::GetPSSource(), ASCIIgL::ShaderType::Pixel, "main", &includes);

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