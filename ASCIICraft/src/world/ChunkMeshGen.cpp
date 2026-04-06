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

namespace {

static constexpr int SIZE = CHUNK_SIZE;
static std::mutex g_missingModelWarnMutex;
static std::unordered_set<uint32_t> g_missingModelWarnedStateIds;

static int GetBlockIndex(int x, int y, int z) {
    return x + y * SIZE + z * SIZE * SIZE;
}

static bool IsValidBlockCoord(int x, int y, int z) {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE && z >= 0 && z < SIZE;
}

static uint32_t GetBlockStateAt(
    int x, int y, int z,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks
) {
    if (IsValidBlockCoord(x, y, z)) {
        return chunkBlocks[GetBlockIndex(x, y, z)];
    }
    int neighborChunkDir = -1;
    int localX = x, localY = y, localZ = z;
    if (x < 0) {
        neighborChunkDir = 5;
        localX = SIZE - 1;
    } else if (x >= SIZE) {
        neighborChunkDir = 4;
        localX = 0;
    } else if (y < 0) {
        neighborChunkDir = 1;
        localY = SIZE - 1;
    } else if (y >= SIZE) {
        neighborChunkDir = 0;
        localY = 0;
    } else if (z < 0) {
        neighborChunkDir = 3;
        localZ = SIZE - 1;
    } else if (z >= SIZE) {
        neighborChunkDir = 2;
        localZ = 0;
    }
    if (neighborChunkDir >= 0 && neighborBlocks[neighborChunkDir]) {
        return neighborBlocks[neighborChunkDir][GetBlockIndex(localX, localY, localZ)];
    }
    return blockstate::BlockStateRegistry::AIR_STATE_ID;
}

static void ComputeVisibleFacesFullBlock(
    int x, int y, int z,
    const blockstate::BlockState& state,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    const blockstate::BlockStateRegistry& bsr,
    std::vector<bool>& visibleFaces
) {
    visibleFaces.resize(6, false);

    for (int face = 0; face < 6; ++face) {
        int neighborX = x, neighborY = y, neighborZ = z;
        switch (face) {
            case 0: neighborY++; break;
            case 1: neighborY--; break;
            case 2: neighborZ++; break;
            case 3: neighborZ--; break;
            case 4: neighborX++; break;
            case 5: neighborX--; break;
        }

        uint32_t neighborStateId = GetBlockStateAt(
            neighborX, neighborY, neighborZ,
            chunkBlocks, neighborBlocks
        );

        const auto& neighborState = bsr.GetState(neighborStateId);

        const bool baseNeighborOccludes =
            (neighborState.isRenderable && neighborState.renderMode == blockstate::RenderMode::Opaque) ||
            (state.cullSameType && neighborState.typeId == state.typeId);
        const bool neighborOccludes = baseNeighborOccludes && neighborState.isFullBlock;

        visibleFaces[face] = !neighborOccludes;
    }
}

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
    const blockstate::BlockModelLibrary* modelLibrary
) {
    ChunkMeshData out;
    if (!chunkBlocks || !bsr || !modelLibrary) return out;

    std::vector<bool> visibleFaces;
    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                uint32_t stateId = chunkBlocks[GetBlockIndex(x, y, z)];
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
                    static_cast<float>(coord.x * SIZE + x),
                    static_cast<float>(coord.y * SIZE + y),
                    static_cast<float>(coord.z * SIZE + z)
                );

                visibleFaces.clear();
                if (model->isFullBlock) {
                    ComputeVisibleFacesFullBlock(x, y, z, state, chunkBlocks, neighborBlocks, *bsr, visibleFaces);
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