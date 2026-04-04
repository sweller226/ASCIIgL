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
static constexpr int FACE_COUNT = 6;
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

// Helpers for BlockModelLibrary-based meshing.

static std::array<bool, FACE_COUNT> ComputeVisibleFaces(
    int x, int y, int z,
    const blockstate::BlockState& state,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, FACE_COUNT>& neighborBlocks,
    const blockstate::BlockStateRegistry& bsr
    // modelLibrary parameter removed
) {
    std::array<bool, FACE_COUNT> visible{};
    visible.fill(false);

    for (int face = 0; face < FACE_COUNT; ++face) {
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

        visible[face] = !neighborOccludes;
    }

    return visible;
}

static void AppendTranslatedVerts_PosUVLayerLight(
    std::vector<std::byte>& dst,
    const std::vector<std::byte>& src,
    const glm::vec3& worldOffset
) {
    using V = ASCIIgL::VertStructs::PosUVLayerLight;
    if (src.empty()) return;
    if (src.size() % sizeof(V) != 0) return; // invalid, ignore

    const size_t vertCount = src.size() / sizeof(V);
    const size_t oldSize = dst.size();
    dst.resize(oldSize + src.size());

    const auto* srcVerts = reinterpret_cast<const V*>(src.data());
    auto* dstVerts = reinterpret_cast<V*>(dst.data() + oldSize);

    for (size_t i = 0; i < vertCount; ++i) {
        V v = srcVerts[i];
        glm::vec3 p = v.GetXYZ();
        p += worldOffset;
        v.SetXYZ(p);
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

static void AppendBlockFace(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const std::vector<std::byte>& modelVerts,
    const std::vector<int>& modelIndices,
    int vertStart,
    int idxStart,
    const glm::vec3& worldOffset
) {
    using V = ASCIIgL::VertStructs::PosUVLayerLight;
    constexpr int kVertsPerFace = 4;
    constexpr int kIndicesPerFace = 6;

    if (modelVerts.size() % sizeof(V) != 0) return;
    if (vertStart < 0 || idxStart < 0) return;

    const int srcVertCount = static_cast<int>(modelVerts.size() / sizeof(V));
    const int srcIndexCount = static_cast<int>(modelIndices.size());
    if (vertStart + kVertsPerFace > srcVertCount) return;
    if (idxStart + kIndicesPerFace > srcIndexCount) return;

    const int baseVertex = static_cast<int>(dstVerts.size() / sizeof(V));
    const auto* srcVerts = reinterpret_cast<const V*>(modelVerts.data()) + vertStart;

    const size_t oldSize = dstVerts.size();
    dstVerts.resize(oldSize + sizeof(V) * kVertsPerFace);
    auto* dstV = reinterpret_cast<V*>(dstVerts.data() + oldSize);

    for (int i = 0; i < kVertsPerFace; ++i) {
        V v = srcVerts[i];
        v.SetXYZ(v.GetXYZ() + worldOffset);
        dstV[i] = v;
    }

    for (int i = 0; i < kIndicesPerFace; ++i) {
        const int localIdx = modelIndices[idxStart + i] - vertStart;
        dstIndices.push_back(baseVertex + localIdx);
    }
}

static void AppendBlock(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const std::vector<std::byte>& modelVerts,
    const std::vector<int>& modelIndices,
    const glm::vec3& worldOffset,
    const std::array<bool, FACE_COUNT> &visibleFaces
) {
    if (modelVerts.empty() || modelIndices.empty()) return;

    for (int face = 0; face < 6; ++face) {
        if (!visibleFaces[face]) continue;

        constexpr int kVertsPerFace = 4; // current cube mapping at callsite (not in helper)
        constexpr int kIndicesPerFace = 6;
        const int faceVertStart = face * kVertsPerFace;
        const int faceIdxStart = face * kIndicesPerFace;

        AppendBlockFace(
            dstVerts,
            dstIndices,
            modelVerts,
            modelIndices,
            faceVertStart,
            faceIdxStart,
            worldOffset
        );
    }
}

static void AppendModel(
    std::vector<std::byte>& dstVerts,
    std::vector<int>& dstIndices,
    const std::vector<std::byte>& modelVerts,
    const std::vector<int>& modelIndices,
    const glm::vec3& worldOffset
) {
    using V = ASCIIgL::VertStructs::PosUVLayerLight;
    if (modelVerts.empty() || modelIndices.empty()) return;
    if (modelVerts.size() % sizeof(V) != 0) return;

    const int baseVertex = static_cast<int>(dstVerts.size() / sizeof(V));
    AppendTranslatedVerts_PosUVLayerLight(dstVerts, modelVerts, worldOffset);
    AppendIndicesRebased(dstIndices, modelIndices, baseVertex);
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

    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                uint32_t stateId = chunkBlocks[GetBlockIndex(x, y, z)];
                const blockstate::BlockState& state = bsr->GetState(stateId);
                if (!state.isRenderable) continue;

                const bool blockIsTranslucent = (state.renderMode == blockstate::RenderMode::Translucent);

                blockstate::BlockModelLibrary::ModelPtr modelPtr = modelLibrary->GetModel(stateId);
                const blockstate::BlockModel* model = modelPtr.get();
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
                

                if (model->isFullBlock) {
                    const std::array<bool, FACE_COUNT> visibleFaces = ComputeVisibleFaces(
                        x, y, z, state, chunkBlocks, neighborBlocks, *bsr
                    );

                    if (blockIsTranslucent) {
                        AppendBlock(
                            out.transparentVertices,
                            out.transparentIndices,
                            model->transparentVertices,
                            model->transparentIndices,
                            worldOffset,
                            visibleFaces
                        );
                        
                    } else {
                        AppendBlock(
                            out.opaqueVertices,
                            out.opaqueIndices,
                            model->opaqueVertices,
                            model->opaqueIndices,
                            worldOffset,
                            visibleFaces
                        );
                    }
                } else {
                    if (blockIsTranslucent) {
                        AppendModel(
                            out.transparentVertices,
                            out.transparentIndices,
                            model->transparentVertices,
                            model->transparentIndices,
                            worldOffset
                        );
                    } else {
                        AppendModel(
                            out.opaqueVertices,
                            out.opaqueIndices,
                            model->opaqueVertices,
                            model->opaqueIndices,
                            worldOffset
                        );
                    }
                }
            }
        }
    }

    return out;
}
