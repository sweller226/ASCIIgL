#pragma once

#include <memory>

namespace ASCIIgL {
class Mesh;
class Texture;
}

namespace gui {

class GUIQuadMeshBuilder {
public:
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(ASCIIgL::Texture* texture);
    static std::shared_ptr<ASCIIgL::Mesh> BuildPosUVQuad(const std::shared_ptr<ASCIIgL::Texture>& texture);
};

} // namespace gui
