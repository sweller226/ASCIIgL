#pragma once

#include <memory>
#include <glm/vec4.hpp>

namespace ASCIIgL {
class Mesh;
class Texture;
}

namespace util {

/// Convert a top-left origin pixel sub-rect in a square atlas to uvRect for BuildPosUVQuad.
/// Pixel coords match PNG / Minecraft atlases (origin top-left). D3D11 uses V=0 at the texture top.
/// Returns (minU, bottomV, maxU, topV) for BuildPosUVQuad's vertex layout (bottom vertex uses .y, top uses .w).
glm::vec4 PixelRectToUV(float atlasSize, int x, int y, int w, int h);

class QuadMeshBuilder {
public:
    /// Standard textured quad (XYZ + UV), sampling the full texture (0-1).
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(ASCIIgL::Texture* texture);
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture);

    /// Textured quad sampling a sub-rect of the atlas. uvRect = (minU, minV, maxU, maxV); see PixelRectToUV.
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(ASCIIgL::Texture* texture, const glm::vec4& uvRect);
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture, const glm::vec4& uvRect);

    /// Texture array quad (XYZ + UV + Layer)
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVLayerQuad(ASCIIgL::Texture* texture, float layer);
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVLayerQuad(const std::shared_ptr<ASCIIgL::Texture>& texture, float layer);

    /// Colored quad (XYZ + RGBA), no texture
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosColorQuad(const glm::vec4& color);
};

} // namespace util