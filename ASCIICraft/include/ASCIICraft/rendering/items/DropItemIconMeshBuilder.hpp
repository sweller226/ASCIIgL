#pragma once

#include <memory>

namespace ASCIIgL {
class Mesh;
class Texture;
}

namespace rendering::items {

class DropItemIconMeshBuilder {
public:
    static std::shared_ptr<ASCIIgL::Mesh> Build(const std::shared_ptr<ASCIIgL::Texture>& texture);
};

} // namespace rendering::items
