#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>

namespace ecs::systems {
namespace {

void SyncDroppedItemFogParams(entt::registry& registry) {
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

    for (const char* materialName : {"droppedItemMaterial", "droppedItemBlockMaterial"}) {
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

RenderSystem::RenderSystem(entt::registry& registry)
    : m_registry(registry) {}

void RenderSystem::Render() {
    SyncDroppedItemFogParams(m_registry);

    auto view = m_registry.view<components::Transform, components::Renderable>();

    for (auto [ent, t, r] : view.each()) {
        if (!r.visible || !r.mesh || !r.material) continue;

        glm::mat4 mvp = glm::mat4(1.0f);

        if (r.renderType == components::RenderType::ELEM_3D) {
            glm::mat4 model = r.billboard
                ? m_active3DCamera->camera.GetBillboardMatrix(t.renderPosition, t.scale)
                : t.getRenderModel() * r.localModel;
            if (m_active3DCamera)
                mvp = m_active3DCamera->camera.proj * m_active3DCamera->camera.view * model;

            ASCIIgL::Renderer::DrawCall dc;
            dc.mesh            = r.mesh.get();
            dc.material        = r.material.get();
            dc.layer           = r.layer;
            dc.transparent     = false;
            dc.sortKey         = 0.0f;
            dc.backfaceCulling = r.backfaceCulling;
            dc.transparent = r.transparent;

            if (const ASCIIgL::UniformDescriptor* desc = r.material->GetUniformDescriptor("mvp"))
                dc.overrides.push_back({ desc, ASCIIgL::UniformValue(mvp) });

            dc.overrides.insert(dc.overrides.end(), r.overrides.begin(), r.overrides.end());

            ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
        } 
        
        else if (r.renderType == components::RenderType::ELEM_2D) {
            if (m_active2DCamera)
                mvp = m_active2DCamera->proj * m_active2DCamera->view * t.getRenderModel();

            ASCIIgL::Renderer::DrawCall dc;
            dc.mesh            = r.mesh.get();
            dc.material        = r.material.get();
            dc.layer           = r.layer;
            dc.transparent     = true;
            dc.sortKey         = static_cast<float>(r.layer);
            dc.backfaceCulling = r.backfaceCulling;

            if (const ASCIIgL::UniformDescriptor* desc = r.material->GetUniformDescriptor("mvp"))
                dc.overrides.push_back({ desc, ASCIIgL::UniformValue(mvp) });

            dc.overrides.insert(dc.overrides.end(), r.overrides.begin(), r.overrides.end());

            ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
        }
    }
}

void RenderSystem::SetActive3DCamera(components::PlayerCamera* camera3D) {
    m_active3DCamera = camera3D;
}

void RenderSystem::SetActive2DCamera(ASCIIgL::Camera2D* camera2D) {
    m_active2DCamera = camera2D;
}

} // namespace ecs::systems
