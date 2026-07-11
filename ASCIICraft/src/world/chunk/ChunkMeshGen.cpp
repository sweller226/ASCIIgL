#include <ASCIICraft/world/chunk/ChunkMeshGen.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <mutex>

#include <glm/glm.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/world/block/models/BlockModelMeshBuilder.hpp>
#include <ASCIICraft/world/block/state/BlockState.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkUtil.hpp>

namespace {

static std::mutex g_missingModelWarnMutex;
static std::unordered_set<uint32_t> g_missingModelWarnedStateIds;

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

                if (blockIsTranslucent) {
                    blockmodels::AppendRenderLayer(
                        out.transparentVertices, out.transparentIndices,
                        model->transparent, worldOffset, visibleFaces);
                } else {
                    auto& opaqueVerts = model->opaqueNoCull ? out.opaqueNoCullVertices : out.opaqueVertices;
                    auto& opaqueIndices = model->opaqueNoCull ? out.opaqueNoCullIndices : out.opaqueIndices;
                    blockmodels::AppendRenderLayer(
                        opaqueVerts, opaqueIndices, model->opaque, worldOffset, visibleFaces);

                    if (!model->transparent.faces.empty()) {
                        blockmodels::AppendRenderLayer(
                            opaqueVerts, opaqueIndices, model->transparent, worldOffset, visibleFaces);
                    }
                }
            }
        }
    }

    return out;
}