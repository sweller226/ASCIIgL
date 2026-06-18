#include <ASCIICraft/gui/GUIRenderer.hpp>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gui {

GUIRenderer::GUIRenderer(ASCIIgL::Camera2D& camera2D)
    : m_camera2D(camera2D) {}

void GUIRenderer::RenderGUIQuad(glm::vec2 topLeftPx,
                                glm::vec2 sizePx,
                                int layer,
                                const std::shared_ptr<ASCIIgL::Mesh>& mesh,
                                const std::shared_ptr<ASCIIgL::Material>& material,
                                const bool meshUsesZeroToOneBounds,
                                const glm::mat4& localModel) const {
    // QuadMeshBuilder uses a -1..1 unit square; scale by half-size and place at rect center.
    const glm::vec2 topLeft = glm::round(topLeftPx);
    const glm::vec2 halfSize = sizePx * 0.5f;
    const glm::vec2 center = topLeft + halfSize;

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(center.x, center.y, 0.0f));
    // Block meshes are Y-up (world); Camera2D is Y-down — flip when drawing 0..1 block bounds.
    const float yHalf = meshUsesZeroToOneBounds ? -halfSize.y : halfSize.y;
    model = glm::scale(model, glm::vec3(halfSize.x, yHalf, halfSize.x));
    if (meshUsesZeroToOneBounds) {
        // Map 0..1 block space to -1..1 centered on the slot: v' = 2*(v - 0.5).
        // GLM gtc transforms post-multiply (m = m * op), so build scale then translate.
        glm::mat4 blockFromUnit(1.0f);
        blockFromUnit = glm::scale(blockFromUnit, glm::vec3(2.0f));
        blockFromUnit = glm::translate(blockFromUnit, glm::vec3(-0.5f, -0.5f, -0.5f));
        model = model * localModel * blockFromUnit;
    }

    SubmitMesh(model, layer, mesh, material);
}

void GUIRenderer::RenderTextMesh(glm::vec2 topLeftPx,
                                 int layer,
                                 const std::shared_ptr<ASCIIgL::Mesh>& textMesh) const {
    auto textMaterial = ASCIIgL::MaterialLibrary::GetInst().Get("guiTextMaterial");
    if (!textMaterial) return;

    const glm::vec2 topLeft = glm::round(topLeftPx);

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(topLeft.x, topLeft.y, 0.0f));
    SubmitMesh(model, layer, textMesh, textMaterial);
}

void GUIRenderer::SubmitMesh(const glm::mat4& model,
                             int layer,
                             const std::shared_ptr<ASCIIgL::Mesh>& mesh,
                             const std::shared_ptr<ASCIIgL::Material>& material) const {
    if (!mesh || !material) return;

    // PosUV quads store the atlas on the mesh; ensure the material's diffuse slot matches.
    if (const ASCIIgL::Texture* meshTexture = mesh->GetTexture()) {
        material->SetTexture("diffuseTexture", meshTexture);
    }

    glm::mat4 mvp = m_camera2D.proj * m_camera2D.view * model;

    ASCIIgL::Renderer::DrawCall dc;
    dc.mesh = mesh.get();
    dc.material = material.get();
    dc.layer = layer;
    dc.transparent = true;
    dc.sortKey = static_cast<float>(layer);
    dc.backfaceCulling = false;
    dc.depthTest = false;

    if (const ASCIIgL::UniformDescriptor* desc = material->GetUniformDescriptor("mvp")) {
        dc.overrides.push_back({desc, ASCIIgL::UniformValue(mvp)});
    }

    ASCIIgL::Renderer::GetInst().SubmitDraw(dc);
}

} // namespace gui
