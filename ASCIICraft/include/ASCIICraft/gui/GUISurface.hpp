#pragma once

#include <memory>

namespace ASCIIgL {
class Mesh;
class Material;
class Texture;
}

namespace gui {

struct GUISurface {
    std::shared_ptr<ASCIIgL::Mesh> mesh;
    std::shared_ptr<ASCIIgL::Material> material;

    /// Build a textured quad sampling a top-left origin pixel rect from a square atlas.
    static GUISurface FromAtlasRegion(const std::shared_ptr<ASCIIgL::Texture>& texture,
                                      float atlasSize,
                                      int x, int y, int w, int h);

    /// Build a textured quad sampling the full texture (0–1 UV).
    static GUISurface FromTexture(const std::shared_ptr<ASCIIgL::Texture>& texture);
};

} // namespace gui
