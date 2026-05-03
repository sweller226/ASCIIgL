#pragma once

#include <memory>
#include <glm/vec4.hpp>

namespace ASCIIgL {
class Mesh;
class Texture;
}

namespace util {

class QuadMeshBuilder {
public:
    /// Standard textured quad (XYZ + UV)
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(ASCIIgL::Texture* texture);
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture);

    /// Texture array quad (XYZ + UV + Layer)
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVLayerQuad(ASCIIgL::Texture* texture, float layer);
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVLayerQuad(const std::shared_ptr<ASCIIgL::Texture>& texture, float layer);

    /// Colored quad (XYZ + RGBA), no texture
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosColorQuad(const glm::vec4& color);
};

} // namespace util