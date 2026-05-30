#pragma once

#include <memory>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace ASCIIgL {
class Camera2D;
class Material;
class Mesh;
}

namespace gui {

/// Thin GUI draw submitter that converts UI-space placement into renderer draw calls.
class GUIRenderer {
public:
    explicit GUIRenderer(ASCIIgL::Camera2D& camera2D);

    void SubmitQuad(glm::vec2 topLeftPx,
                    glm::vec2 sizePx,
                    int layer,
                    const std::shared_ptr<ASCIIgL::Mesh>& mesh,
                    const std::shared_ptr<ASCIIgL::Material>& material) const;

    void SubmitMesh(const glm::mat4& model,
                    int layer,
                    const std::shared_ptr<ASCIIgL::Mesh>& mesh,
                    const std::shared_ptr<ASCIIgL::Material>& material) const;

private:
    ASCIIgL::Camera2D& m_camera2D;
};

} // namespace gui
