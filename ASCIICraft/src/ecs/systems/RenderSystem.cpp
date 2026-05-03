#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace ecs::systems {

RenderSystem::RenderSystem(entt::registry& registry)
    : m_registry(registry) {}

void RenderSystem::BeginFrame() {
    m_drawList3D.clear();
    m_drawList2D.clear();
}

void RenderSystem::Render() {
    CollectVisible();

    std::stable_sort(m_drawList3D.begin(), m_drawList3D.end(),
                     [](const DrawItem& a, const DrawItem& b) { return a.layer < b.layer; });
    std::stable_sort(m_drawList2D.begin(), m_drawList2D.end(),
                     [](const DrawItem& a, const DrawItem& b) { return a.layer < b.layer; });

    BatchAndDraw();
}

void RenderSystem::CollectVisible() {
    auto view = m_registry.view<components::Transform, components::Renderable>();

    for (auto [ent, t, r] : view.each()) {
        if (!r.visible || !r.mesh) continue;

        DrawItem item;
        item.mesh         = r.mesh;
        item.material     = r.material;
        item.materialName = "";
        item.modelMatrix  = t.getRenderModel();
        item.layer        = r.layer;
        item.overrides    = r.overrides; // carry per-entity overrides

        if (r.renderType == components::RenderType::ELEM_3D)
            m_drawList3D.push_back(std::move(item));
        else if (r.renderType == components::RenderType::ELEM_2D)
            m_drawList2D.push_back(std::move(item));
    }
}

void RenderSystem::BatchAndDraw() {
    auto ResolveMaterial = [](const DrawItem& item) -> std::shared_ptr<ASCIIgL::Material> {
        if (item.material) return item.material;
        if (!item.materialName.empty())
            if (auto m = ASCIIgL::MaterialLibrary::GetInst().Get(item.materialName)) return m;
        return ASCIIgL::MaterialLibrary::GetInst().GetDefault();
    };

    for (const auto& item : m_drawList3D) {
        auto mat = ResolveMaterial(item);
        if (!mat) continue;

        glm::mat4 mvp = glm::mat4(1.0f);
        if (m_active3DCamera)
            mvp = m_active3DCamera->camera.proj * m_active3DCamera->camera.view * item.modelMatrix;

        ASCIIgL::Renderer::DrawCall dc;
        dc.mesh        = item.mesh.get();
        dc.material    = mat.get();
        dc.layer       = item.layer;
        dc.transparent = false;
        dc.sortKey     = 0.0f;

        if (const ASCIIgL::UniformDescriptor* desc = mat->GetUniformDescriptor("mvp"))
            dc.overrides.push_back({ desc, ASCIIgL::UniformValue(mvp) });

        dc.overrides.insert(dc.overrides.end(), item.overrides.begin(), item.overrides.end());

        ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
    }

    if (!m_active2DCamera && !m_drawList2D.empty())
        ASCIIgL::Logger::Warning("RenderSystem: drawing 2D entities with null camera");

    for (const auto& item : m_drawList2D) {
        auto mat = ResolveMaterial(item);
        if (!mat) continue;

        glm::mat4 mvp = glm::mat4(1.0f);
        if (m_active2DCamera)
            mvp = m_active2DCamera->proj * m_active2DCamera->view * item.modelMatrix;

        ASCIIgL::Renderer::DrawCall dc;
        dc.mesh        = item.mesh.get();
        dc.material    = mat.get();
        dc.layer       = item.layer;
        dc.transparent = true;
        dc.sortKey     = static_cast<float>(item.layer);

        if (const ASCIIgL::UniformDescriptor* desc = mat->GetUniformDescriptor("mvp"))
            dc.overrides.push_back({ desc, ASCIIgL::UniformValue(mvp) });

        dc.overrides.insert(dc.overrides.end(), item.overrides.begin(), item.overrides.end());

        ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
    }
}

void RenderSystem::SetActive3DCamera(components::PlayerCamera* camera3D) {
    m_active3DCamera = camera3D;
}

void RenderSystem::SetActive2DCamera(ASCIIgL::Camera2D* camera2D) {
    m_active2DCamera = camera2D;
}

} // namespace ecs::systems