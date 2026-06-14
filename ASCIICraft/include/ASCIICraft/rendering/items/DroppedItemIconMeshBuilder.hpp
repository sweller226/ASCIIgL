#pragma once

#include <memory>

namespace ASCIIgL {
class Mesh;
class TextureArray;
}

namespace rendering::items {

class DroppedItemIconMeshBuilder {
public:
    static std::shared_ptr<ASCIIgL::Mesh> Build(
        float iconLayer,
        const std::shared_ptr<ASCIIgL::TextureArray>& textureArray
    );
};

} // namespace rendering::items
