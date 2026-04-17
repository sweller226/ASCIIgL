#include <ASCIICraft/world/ChunkMeshGen.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <mutex>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/world/blockstate/BlockState.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/FaceCulling.hpp>
#include <ASCIICraft/world/ChunkUtil.hpp>

namespace {

static std::mutex g_missingModelWarnMutex;
static std::unordered_set<uint32_t> g_missingModelWarnedStateIds;

static void AppendTranslatedVerts_PosUVLayerLight(
    std::vector<std::byte>& dst,
    const std::vector<std::byte>& src,
    const glm::vec3& worldOffset
) {
    using V = ASCIIgL::VertStructs::PosUVLayerLight;
    if (src.empty()) return;
    if (src.size() % sizeof(V) != 0) return;

    const size_t vertCount = src.size() / sizeof(V);
    const size_t oldSize = dst.size();
    dst.resize(oldSize + src.size());

    const auto* srcVerts = reinterpret_cast<const V*>(src.data());
    auto* dstVerts = reinterpret_cast<V*>(dst.data() + oldSize);

    for (size_t i = 0; i < vertCount; ++i) {
        V v = srcVerts[i];
        v.SetXYZ(v.GetXYZ() + worldOffset);
        dstVerts[i] = v;
    }
}

static void AppendIndicesRebased(
    std::vector<int>& dst,
    const std::vector<int>& src,
    int baseVertex
) {
    dst.reserve(dst.size() + src.size());
    for (int idx : src) dst.push_back(baseVertex + idx);
}

static void AppendFaces(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const blockstate::RenderLayer& layer,
    const glm::vec3& worldOffset,
    const std::vector<bool>& visibleFaces
) {
    using V = ASCIIgL::VertStructs::PosUVLayerLight;
    for (size_t i = 0; i < layer.faces.size(); ++i) {
        if (i < visibleFaces.size() && !visibleFaces[i]) continue;

        const blockstate::FaceRange& f = layer.faces[i];
        const int baseVertex = static_cast<int>(dstVerts.size() / sizeof(V));

        const size_t oldSize = dstVerts.size();
        dstVerts.resize(oldSize + f.vertByteCount);
        const auto* src = reinterpret_cast<const V*>(layer.vertices.data() + f.vertByteOffset);
        auto* dst       = reinterpret_cast<V*>(dstVerts.data() + oldSize);
        const int vertCount = f.vertByteCount / sizeof(V);
        for (int j = 0; j < vertCount; ++j) {
            V v = src[j];
            v.SetXYZ(v.GetXYZ() + worldOffset);
            dst[j] = v;
        }

        for (int j = 0; j < f.idxCount; ++j)
            dstIndices.push_back(baseVertex + layer.indices[f.idxOffset + j]);
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

                const blockstate::BlockModel* model = modelLibrary->GetModel(stateId);
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
                    static_cast<float>(coord.x * sizes::CHUNK_SIZE + x),
                    static_cast<float>(coord.y * sizes::CHUNK_SIZE + y),
                    static_cast<float>(coord.z * sizes::CHUNK_SIZE + z)
                );

                visibleFaces.clear();
                if (model->computeVisibleFaces) {
                    model->computeVisibleFaces(x, y, z, state, chunkBlocks, neighborBlocks, *bsr, visibleFaces);
                }

                auto& dstVerts   = blockIsTranslucent ? out.transparentVertices : out.opaqueVertices;
                auto& dstIndices = blockIsTranslucent ? out.transparentIndices  : out.opaqueIndices;
                const blockstate::RenderLayer& layer = blockIsTranslucent ? model->transparent : model->opaque;
                AppendFaces(dstVerts, dstIndices, layer, worldOffset, visibleFaces);
            }
        }
    }

    return out;
}