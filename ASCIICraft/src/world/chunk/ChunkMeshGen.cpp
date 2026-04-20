#include <ASCIICraft/world/chunk/ChunkMeshGen.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <mutex>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/block/FaceCulling.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

#include <algorithm>

namespace {

static std::mutex g_missingModelWarnMutex;
static std::unordered_set<uint32_t> g_missingModelWarnedStateIds;

static void AppendFaces(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const blockstate::RenderLayer& layer,
    const glm::vec3& worldOffset,
    const std::vector<bool>& visibleFaces
) {
    using V = ASCIIgL::VertStructs::PosUVLayerLight;
    constexpr unsigned kFaceUnset = 255;
    // Scratch typed storage: keeps vertex math aligned/typed while public buffers remain byte-packed.
    std::vector<V> scratchVerts;
    for (size_t i = 0; i < layer.faces.size(); ++i) {
        const blockstate::FaceRange& f = layer.faces[i];
        // Neighbor culling masks by world direction (six faces). JSON models emit quads in arbitrary order,
        // so we mask using cardinalFace on each FaceRange — not the index within layer.faces.
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
        const int vertCount = f.vertByteCount / sizeof(V);
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
            v.SetXYZ(v.GetXYZ() + worldOffset);
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

} // namespace

ChunkMeshData BuildChunkMeshData(
    ChunkCoord coord,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    const blockstate::BlockStateRegistry* bsr,
    const blockmodels::BlockModelLibrary* modelLibrary
) {
    ChunkMeshData out;
    if (!chunkBlocks || !bsr || !modelLibrary) return out;

    std::vector<bool> visibleFaces;
    for (int x = 0; x < sizes::CHUNK_SIZE; ++x) {
        for (int y = 0; y < sizes::CHUNK_SIZE; ++y) {
            for (int z = 0; z < sizes::CHUNK_SIZE; ++z) {
                uint32_t stateId = chunkBlocks[chunkutil::GetBlockIndex(x, y, z)];
                const blockstate::BlockState& state = bsr->GetState(stateId);
                if (!state.isRenderable) continue;

                const bool blockIsTranslucent = (state.renderMode == blockstate::RenderMode::Translucent);

                const int worldX = coord.x * sizes::CHUNK_SIZE + x;
                const int worldY = coord.y * sizes::CHUNK_SIZE + y;
                const int worldZ = coord.z * sizes::CHUNK_SIZE + z;

                const blockstate::BlockModel* model = modelLibrary->GetModelForBlock(stateId, worldX, worldY, worldZ);
                if (!model) {
                    std::lock_guard<std::mutex> lock(g_missingModelWarnMutex);
                    if (g_missingModelWarnedStateIds.insert(stateId).second) {
                        ASCIIgL::Logger::Warningf(
                            "BuildChunkMeshData: missing BlockModel for stateId=%u. Skipping block.",
                            static_cast<unsigned>(stateId)
                        );
                    }
                    continue;
                }

                const glm::vec3 worldOffset(
                    static_cast<float>(worldX),
                    static_cast<float>(worldY),
                    static_cast<float>(worldZ)
                );

                visibleFaces.clear();
                if (model->computeVisibleFaces) {
                    model->computeVisibleFaces(x, y, z, state, chunkBlocks, neighborBlocks, *bsr, visibleFaces);
                }

                auto& dstVerts = blockIsTranslucent
                    ? out.transparentVertices
                    : (model->opaqueNoCull ? out.opaqueNoCullVertices : out.opaqueVertices);
                auto& dstIndices = blockIsTranslucent
                    ? out.transparentIndices
                    : (model->opaqueNoCull ? out.opaqueNoCullIndices : out.opaqueIndices);
                const blockstate::RenderLayer& layer = blockIsTranslucent ? model->transparent : model->opaque;
                AppendFaces(dstVerts, dstIndices, layer, worldOffset, visibleFaces);
            }
        }
    }

    return out;
}