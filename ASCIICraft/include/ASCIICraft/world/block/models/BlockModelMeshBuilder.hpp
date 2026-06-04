#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <glm/vec3.hpp>

namespace ASCIIgL {
class Mesh;
class TextureArray;
}

namespace blockstate {
struct BlockModel;
struct RenderLayer;
}

namespace blockmodels {

/// Append one RenderLayer into destination buffers (optionally culled by visibleFaces).
void AppendRenderLayer(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const blockstate::RenderLayer& layer,
    glm::vec3 positionOffset = glm::vec3(0.0f),
    const std::vector<bool>& visibleFaces = {}
);

struct BlockModelMeshBuffers {
    std::vector<std::byte> vertices;
    std::vector<int> indices;

    bool empty() const { return vertices.empty() || indices.empty(); }
};

/// Mesh a single BlockModel (opaque + transparent, all faces, no neighbor culling).
BlockModelMeshBuffers BuildMeshBuffers(
    const blockstate::BlockModel& model,
    glm::vec3 positionOffset = glm::vec3(0.0f)
);

std::shared_ptr<ASCIIgL::Mesh> BuildMesh(
    const blockstate::BlockModel& model,
    ASCIIgL::TextureArray* textureArray,
    glm::vec3 positionOffset = glm::vec3(0.0f)
);

} // namespace blockmodels
