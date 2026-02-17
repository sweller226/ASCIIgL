#include <ASCIICraft/ecs/systems/RenderSystem.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Shader.hpp>   // UniformDescriptor, UniformValue
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
                         [](const DrawItem& a, const DrawItem& b) {
                             return a.layer < b.layer;
                         });

        std::stable_sort(m_drawList2D.begin(), m_drawList2D.end(),
                         [](const DrawItem& a, const DrawItem& b) {
                             return a.layer < b.layer;
                         });

        // Diagnostic: when we have 2D items, log first item and camera so we can verify GUI is in view
        if (!m_drawList2D.empty() && m_active2DCamera) {
            const auto& first = m_drawList2D.front();
            ASCIIgL::Logger::Debug("2D draw: count=" + std::to_string(m_drawList2D.size()) +
                " first pos=(" + std::to_string(first.modelMatrix[3][0]) + "," + std::to_string(first.modelMatrix[3][1]) + ")" +
                " scale=(" + std::to_string(first.modelMatrix[0][0]) + "," + std::to_string(first.modelMatrix[1][1]) + ")");
        }

        // Batch by mesh pointer to reduce state changes
        BatchAndDraw();
    }

    void RenderSystem::CollectVisible() {
        // View for entities that have both Transform and Renderable
        auto view = m_registry.view<components::Transform, components::Renderable>();

        for (auto [ent, t, r] : view.each()) {
            auto &t = view.get<components::Transform>(ent);
            auto &r = view.get<components::Renderable>(ent);

            if (!r.visible || !r.mesh) continue;
            
            glm::vec3 worldCenter = t.position; // if mesh has offset, add it

            // need to do frustum culling

            DrawItem item;
            item.entity = ent;
            item.mesh = r.mesh;
            item.material = nullptr; // ECS entities don't use materials yet
            item.materialName = "";  // Will use default material
            item.modelMatrix = t.getModel();
            item.layer = r.layer;

            if (r.renderType == components::RenderType::ELEM_3D) {
                m_drawList3D.push_back(std::move(item));
            } else if (r.renderType == components::RenderType::ELEM_2D) {
                m_drawList2D.push_back(std::move(item));
            }
        }
    }

    void RenderSystem::AddGuiItem(glm::vec2 position, glm::vec2 size, int layer,
                                   std::shared_ptr<ASCIIgL::Mesh> mesh,
                                   std::shared_ptr<ASCIIgL::Material> material,
                                   const std::string& materialName) {
        if (!mesh) return;
        DrawItem item;
        item.entity = entt::null;
        item.mesh = std::move(mesh);
        item.material = std::move(material);
        item.materialName = materialName;
        // Widget layout gives top-left; quad is ±1 (extent 2), so scale by half-size then translate by center
        // Use z=0 for 2D GUI (layer is only used for draw order, not depth)
        const float cx = position.x + size.x * 0.5f;
        const float cy = position.y + size.y * 0.5f;
        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(cx, cy, 0.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(size.x * 0.5f, size.y * 0.5f, 1.0f));
        item.modelMatrix = t * s;
        item.layer = layer;
        m_drawList2D.push_back(std::move(item));
    }

    void RenderSystem::BatchAndDraw() {
        // 3D: enqueue as opaque draws
        for (const auto& item : m_drawList3D) {
            const ASCIIgL::Mesh* meshPtr = item.mesh.get();
            if (!meshPtr) continue;

            // Get material: use item.material if provided, else fall back to materialName lookup
            std::shared_ptr<ASCIIgL::Material> mat = item.material;
            if (!mat && !item.materialName.empty()) {
                mat = ASCIIgL::MaterialLibrary::GetInst().Get(item.materialName);
            }
            if (!mat) mat = ASCIIgL::MaterialLibrary::GetInst().GetDefault();
            if (!mat) continue;

            ASCIIgL::Renderer::DrawCall dc;
            dc.mesh        = meshPtr;
            dc.material    = mat.get();
            dc.layer       = item.layer;
            dc.transparent = false;       // world geometry is opaque by default
            dc.sortKey     = 0.0f;        // not needed for opaque

            // Compute MVP for 3D
            glm::mat4 mvp = glm::mat4(1.0f);
            if (m_active3DCamera) {
                mvp = m_active3DCamera->camera.proj *
                      m_active3DCamera->camera.view *
                      item.modelMatrix;
            }

            // Per-draw uniform override for MVP
            if (const ASCIIgL::UniformDescriptor* mvpDesc = mat->GetUniformDescriptor("mvp")) {
                ASCIIgL::Renderer::UniformOverride ov;
                ov.desc  = mvpDesc;
                ov.value = ASCIIgL::UniformValue(mvp);
                dc.overrides.push_back(std::move(ov));
            }

            ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
        }

        // 2D GUI: enqueue as transparent draws so they are rendered after opaque
        if (!m_active2DCamera && !m_drawList2D.empty()) {
            ASCIIgL::Logger::Warning("RenderSystem: drawing 2D with null camera (GUI may be wrong)");
        }
        for (const auto& item : m_drawList2D) {
            const ASCIIgL::Mesh* meshPtr = item.mesh.get();
            if (!meshPtr) continue;
            
            // Get material: use item.material if provided, else fall back to materialName lookup
            std::shared_ptr<ASCIIgL::Material> mat = item.material;
            if (!mat && !item.materialName.empty()) {
                mat = ASCIIgL::MaterialLibrary::GetInst().Get(item.materialName);
            }
            if (!mat) mat = ASCIIgL::MaterialLibrary::GetInst().GetDefault();
            if (!mat) continue;

            ASCIIgL::Renderer::DrawCall dc;
            dc.mesh        = meshPtr;
            dc.material    = mat.get();
            dc.layer       = item.layer;
            dc.transparent = true;        // GUI/2D treated as transparent pass

            // For now, use layer as sort key for 2D ordering
            dc.sortKey = static_cast<float>(item.layer);

            // Compute MVP for 2D
            glm::mat4 mvp = glm::mat4(1.0f);
            if (m_active2DCamera) {
                mvp = m_active2DCamera->proj *
                      m_active2DCamera->view *
                      item.modelMatrix;
            }

            // Per-draw uniform override for MVP
            if (const ASCIIgL::UniformDescriptor* mvpDesc = mat->GetUniformDescriptor("mvp")) {
                ASCIIgL::Renderer::UniformOverride ov;
                ov.desc  = mvpDesc;
                ov.value = ASCIIgL::UniformValue(mvp);
                dc.overrides.push_back(std::move(ov));
            }

            ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
        }
    }

void RenderSystem::SetActive3DCamera(components::PlayerCamera* camera3D) {
    m_active3DCamera = camera3D;
}

void RenderSystem::SetActive2DCamera(ASCIIgL::Camera2D* camera2D) {
    m_active2DCamera = camera2D;
}

}

