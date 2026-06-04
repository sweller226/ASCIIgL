#include <ASCIICraft/world/block/models/BlockModelMeshBuilder.hpp>

#include <ASCIICraft/world/block/models/BlockModel.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <cstring>
#include <vector>

namespace blockmodels {

void AppendRenderLayer(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const blockstate::RenderLayer& layer,
    glm::vec3 positionOffset,
    const std::vector<bool>& visibleFaces
) {
    using V = ASCIIgL::VertStructs::PosUVLayer;
    constexpr unsigned kFaceUnset = 255;

    std::vector<V> scratchVerts;
    for (size_t i = 0; i < layer.faces.size(); ++i) {
        const blockstate::FaceRange& f = layer.faces[i];
        if (!visibleFaces.empty()) {
            if (f.cardinalFace != kFaceUnset && f.cardinalFace < visibleFaces.size()) {
                if (!visibleFaces[f.cardinalFace]) {
                    continue;
                }
            }
        }

        const int baseVertex = static_cast<int>(dstVerts.size() / sizeof(V));

        if (f.vertByteCount <= 0 || (f.vertByteCount % static_cast<int>(sizeof(V)) != 0)) {
            continue;
        }
        const size_t oldSize = dstVerts.size();
        const int vertCount = f.vertByteCount / static_cast<int>(sizeof(V));
        if (vertCount <= 0) {
            continue;
        }

        scratchVerts.resize(static_cast<size_t>(vertCount));
        std::memcpy(
            scratchVerts.data(),
            layer.vertices.data() + static_cast<size_t>(f.vertByteOffset),
            static_cast<size_t>(f.vertByteCount)
        );
        for (V& v : scratchVerts) {
            v.SetXYZ(v.GetXYZ() + positionOffset);
        }

        dstVerts.resize(oldSize + static_cast<size_t>(f.vertByteCount));
        std::memcpy(
            dstVerts.data() + oldSize,
            scratchVerts.data(),
            static_cast<size_t>(f.vertByteCount)
        );

        for (int j = 0; j < f.idxCount; ++j) {
            dstIndices.push_back(baseVertex + layer.indices[f.idxOffset + j]);
        }
    }
}

BlockModelMeshBuffers BuildMeshBuffers(const blockstate::BlockModel& model, glm::vec3 positionOffset) {
    BlockModelMeshBuffers out;
    const std::vector<bool> noCull;
    AppendRenderLayer(out.vertices, out.indices, model.opaque, positionOffset, noCull);
    AppendRenderLayer(out.vertices, out.indices, model.transparent, positionOffset, noCull);
    return out;
}

std::shared_ptr<ASCIIgL::Mesh> BuildMesh(
    const blockstate::BlockModel& model,
    ASCIIgL::TextureArray* textureArray,
    glm::vec3 positionOffset
) {
    if (!textureArray) {
        return nullptr;
    }

    BlockModelMeshBuffers buffers = BuildMeshBuffers(model, positionOffset);
    if (buffers.empty()) {
        return nullptr;
    }

    return std::make_shared<ASCIIgL::Mesh>(
        std::move(buffers.vertices),
        ASCIIgL::VertFormats::PosUVLayer(),
        std::move(buffers.indices),
        textureArray
    );
}

} // namespace blockmodels
