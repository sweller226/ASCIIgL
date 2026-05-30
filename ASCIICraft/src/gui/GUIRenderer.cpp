#include <ASCIICraft/gui/GUIRenderer.hpp>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace gui {

GUIRenderer::GUIRenderer(ASCIIgL::Camera2D& camera2D)
    : m_camera2D(camera2D) {}

void GUIRenderer::SubmitQuad(glm::vec2 topLeftPx,
                             glm::vec2 sizePx,
                             int layer,
                             const std::shared_ptr<ASCIIgL::Mesh>& mesh,
                             const std::shared_ptr<ASCIIgL::Material>& material) const {
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(topLeftPx.x, topLeftPx.y, 0.0f));
    model = glm::scale(model, glm::vec3(sizePx.x, sizePx.y, 1.0f));

    SubmitMesh(model, layer, mesh, material);
}

void GUIRenderer::SubmitMesh(const glm::mat4& model,
                             int layer,
                             const std::shared_ptr<ASCIIgL::Mesh>& mesh,
                             const std::shared_ptr<ASCIIgL::Material>& material) const {
    if (!mesh || !material) return;

    glm::mat4 mvp = m_camera2D.proj * m_camera2D.view * model;

    ASCIIgL::Renderer::DrawCall dc;
    dc.mesh = mesh.get();
    dc.material = material.get();
    dc.layer = layer;
    dc.transparent = true;
    dc.sortKey = static_cast<float>(layer);
    dc.backfaceCulling = false;

    if (const ASCIIgL::UniformDescriptor* desc = material->GetUniformDescriptor("mvp")) {
        dc.overrides.push_back({desc, ASCIIgL::UniformValue(mvp)});
    }

    ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
}

} // namespace gui
