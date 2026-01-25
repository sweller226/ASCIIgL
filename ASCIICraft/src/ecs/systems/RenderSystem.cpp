#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

#include <ASCIIgL/renderer/gpu/Material.hpp>
#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

namespace ecs::systems {

    void RenderSystem::Render() {

        // Collect visible renderables into a vector for sorting and batching
        m_drawList3D.clear();
        m_drawList2D.clear();
        CollectVisible();

        // Sort by layer (ascending). Stable sort keeps insertion order for same layer.
        std::stable_sort(m_drawList3D.begin(), m_drawList3D.end(),
                         [](const DrawItem& a, const DrawItem& b) {
                             return a.layer < b.layer;
                         });

        std::stable_sort(m_drawList2D.begin(), m_drawList2D.end(),
                         [](const DrawItem& a, const DrawItem& b) {
                             return a.layer < b.layer;
                         });

        // Batch by mesh pointer to reduce state changes
        BatchAndDraw();
    }

    void RenderSystem::CollectVisible() {
        // View for entities that have both Transform and Renderable
        auto view = m_registry.view<components::Transform, components::Renderable>();

        for (auto [ent, t, v] : view.each()) {
            auto &t = view.get<components::Transform>(ent);
            auto &r = view.get<components::Renderable>(ent);

            if (!r.visible || !r.mesh) continue;
            
            glm::vec3 worldCenter = t.position; // if mesh has offset, add it

            // need to do frustum culling

            DrawItem item;
            item.entity = ent;
            item.mesh = r.mesh;
            item.texture = nullptr; // if Renderable had texture, set it here
            item.modelMatrix = t.getModel();
            item.layer = r.layer;

            if (v.renderType == components::RenderType::ELEM_3D) {
                m_drawList3D.push_back(std::move(item));
            } else if (v.renderType == components::RenderType::ELEM_2D) {
                m_drawList2D.push_back(std::move(item));
            }
        }
    }

    void RenderSystem::BatchAndDraw() {
        const ASCIIgL::Mesh* lastMesh = nullptr;

        for (const auto& item : m_drawList3D) {
            const ASCIIgL::Mesh* meshPtr = item.mesh.get();

            if (meshPtr != lastMesh) {
                auto mat = ASCIIgL::MaterialLibrary::GetInst().GetDefault();
                ASCIIgL::RendererGPU::GetInst().BindMaterial(mat.get());

                glm::mat4 mvp = glm::mat4(1.0f);
                if (m_active3DCamera) {
                    mvp = m_active3DCamera->camera->proj * m_active3DCamera->camera->view * item.modelMatrix;
                }

                mat->SetMatrix4("mvp", mvp);
                ASCIIgL::RendererGPU::GetInst().UploadMaterialConstants(mat.get());
                lastMesh = meshPtr;
            }

            // Issue draw call with model matrix
            ASCIIgL::Renderer::GetInst().DrawMesh(meshPtr);
        }

        for (const auto& item : m_drawList2D) {
            const ASCIIgL::Mesh* meshPtr = item.mesh.get();

            if (meshPtr != lastMesh) {
                auto mat = ASCIIgL::MaterialLibrary::GetInst().GetDefault();
                ASCIIgL::RendererGPU::GetInst().BindMaterial(mat.get());

                glm::mat4 mvp = glm::mat4(1.0f);
                if (m_active2DCamera) {
                    mvp = m_active2DCamera->camera->proj * m_active2DCamera->camera->view * item.modelMatrix;
                }

                mat->SetMatrix4("mvp", mvp);
                ASCIIgL::RendererGPU::GetInst().UploadMaterialConstants(mat.get());
                lastMesh = meshPtr;
            }

            // Issue draw call with model matrix
            ASCIIgL::Renderer::GetInst().DrawMesh(meshPtr);
        }
    }

void RenderSystem::SetActive3DCamera(components::PlayerCamera* camera3D) {
    m_active3DCamera = camera3D;
}

void RenderSystem::SetActive2DCamera(components::GUICamera* camera2D) {
    m_active2DCamera = camera2D;
}

}

