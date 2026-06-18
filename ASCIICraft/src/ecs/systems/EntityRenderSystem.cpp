#include <ASCIICraft/ecs/systems/EntityRenderSystem.hpp>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>

namespace ecs::systems {
namespace {

void SyncEntityFogParams(entt::registry& registry) {
    World* world = GetWorldPtr(registry);
    if (!world || !world->GetChunkManager()) {
        return;
    }

    const ChunkManagerFogParams& fog = world->GetChunkManager()->GetFogParams();
    const glm::vec4 fogParams(fog.fogStart, fog.fogEnd, 0.0f, 0.0f);

    glm::vec3 cameraPos{0.0f};
    const entt::entity player = components::GetPlayerEntity(registry);
    if (player != entt::null) {
        if (auto [pos, ok] = components::GetPos(player, registry); ok) {
            cameraPos = pos;
        }
    }

    for (const char* materialName : {
             "droppedItemMaterial",
             "droppedItemBlockMaterial",
             "leafParticleMaterial",
         }) {
        auto material = ASCIIgL::MaterialLibrary::GetInst().Get(materialName);
        if (!material) {
            continue;
        }
        material->SetFloat3("cameraPos", cameraPos);
        material->SetFloat4("fogParams", fogParams);
        material->SetFloat3("fogColor", fog.fogColor);
    }
}

} // namespace

EntityRenderSystem::EntityRenderSystem(entt::registry& registry)
    : m_registry(registry) {}

void EntityRenderSystem::Render() {
    if (!m_active3DCamera) {
        return;
    }

    SyncEntityFogParams(m_registry);

    auto view = m_registry.view<components::Transform, components::Renderable>();

    for (auto [ent, t, r] : view.each()) {
        if (!r.visible || !r.mesh || !r.material) {
            continue;
        }
        if (r.renderType != components::RenderType::ELEM_3D) {
            continue;
        }

        const glm::mat4 model = r.billboard
            ? m_active3DCamera->camera.GetBillboardMatrix(t.renderPosition, t.scale)
            : t.getRenderModel() * r.localModel;
        const glm::mat4 mvp = m_active3DCamera->camera.proj * m_active3DCamera->camera.view * model;

        ASCIIgL::Renderer::DrawCall dc;
        dc.mesh = r.mesh.get();
        dc.material = r.material.get();
        dc.layer = r.layer;
        dc.sortKey = 0.0f;
        dc.backfaceCulling = r.backfaceCulling;
        dc.transparent = r.transparent;

        if (const ASCIIgL::UniformDescriptor* desc = r.material->GetUniformDescriptor("mvp")) {
            dc.overrides.push_back({desc, ASCIIgL::UniformValue(mvp)});
        }

        if (const ASCIIgL::UniformDescriptor* worldPosDesc = r.material->GetUniformDescriptor("worldPos")) {
            dc.overrides.push_back({worldPosDesc, ASCIIgL::UniformValue(t.renderPosition)});
        }

        dc.overrides.insert(dc.overrides.end(), r.overrides.begin(), r.overrides.end());

        ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
    }
}

void EntityRenderSystem::SetActive3DCamera(components::PlayerCamera* camera3D) {
    m_active3DCamera = camera3D;
}

} // namespace ecs::systems
