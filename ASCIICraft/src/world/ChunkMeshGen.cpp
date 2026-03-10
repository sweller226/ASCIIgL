#include <ASCIICraft/world/ChunkMeshGen.hpp>
#include <ASCIICraft/world/blockstate/BlockFace.hpp>

#include <array>
#include <vector>
#include <algorithm>
#include <numeric>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

namespace {

static constexpr int SIZE = CHUNK_SIZE;

static int GetBlockIndex(int x, int y, int z) {
    return x + y * SIZE + z * SIZE * SIZE;
}

static float TestFaceLightConstant() {
    return 1.0f;
}

static bool IsValidBlockCoord(int x, int y, int z) {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE && z >= 0 && z < SIZE;
}

static uint32_t GetBlockStateAt(
    int x, int y, int z,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    int* outNeighborDir, int* outLocalX, int* outLocalY, int* outLocalZ
) {
    if (IsValidBlockCoord(x, y, z)) {
        if (outNeighborDir) *outNeighborDir = -1;
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
    if (outNeighborDir) *outNeighborDir = neighborChunkDir;
    if (outLocalX) *outLocalX = localX;
    if (outLocalY) *outLocalY = localY;
    if (outLocalZ) *outLocalZ = localZ;
    if (neighborChunkDir >= 0 && neighborBlocks[neighborChunkDir]) {
        return neighborBlocks[neighborChunkDir][GetBlockIndex(localX, localY, localZ)];
    }
    return blockstate::BlockStateRegistry::AIR_STATE_ID;
}

} // namespace

ChunkMeshData BuildChunkMeshData(
    ChunkCoord coord,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    const blockstate::BlockStateRegistry* bsr
) {
    ChunkMeshData out;
    if (!chunkBlocks || !bsr) return out;

    std::vector<std::byte>& verticesOpaque = out.opaqueVertices;
    std::vector<int>& indicesOpaque = out.opaqueIndices;

    struct TransparentFace {
        int x, y, z;
        int faceIndex;
        int textureLayer;
        uint8_t blockFacing;
    };
    std::vector<TransparentFace> transparentFaces;

    const glm::vec3 faceVerticesIndexed[6][4] = {
        { glm::vec3(0, 1, 0), glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0) },
        { glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 0, 1) },
        { glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1) },
        { glm::vec3(1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 0) },
        { glm::vec3(1, 0, 1), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1) },
        { glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(0, 1, 0) }
    };
    const glm::vec2 faceUVsIndexed[4] = {
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)
    };
    const int faceIndices[6] = { 0, 1, 2, 0, 2, 3 };

    // Directional light multipliers.
    const unsigned int stride = static_cast<unsigned int>(ASCIIgL::VertFormats::PosUVLayerLight().GetStride());

    for (int x = 0; x < SIZE; ++x) {
        for (int y = 0; y < SIZE; ++y) {
            for (int z = 0; z < SIZE; ++z) {
                uint32_t stateId = chunkBlocks[GetBlockIndex(x, y, z)];
                const auto& state = bsr->GetState(stateId);
                if (!state.isSolid) continue;

                const bool blockIsTranslucent = (state.renderMode == blockstate::RenderMode::Translucent);
                BlockFace blockFacing = StringToBlockFace(bsr->GetPropertyValue(stateId, "facing"));

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

                    int neighborDir = -1, localX, localY, localZ;
                    uint32_t neighborStateId = GetBlockStateAt(
                        neighborX, neighborY, neighborZ,
                        chunkBlocks, neighborBlocks,
                        &neighborDir, &localX, &localY, &localZ
                    );
                    const auto& neighborState = bsr->GetState(neighborStateId);
                    bool neighborOccludes =
                        neighborState.isSolid &&
                        neighborState.renderMode == blockstate::RenderMode::Opaque;
                    bool shouldRenderFace = !neighborOccludes;

                    if (!shouldRenderFace) continue;

                    int textureLayer = state.faceTextureLayers[face];

                    if (blockIsTranslucent) {
                        transparentFaces.push_back({
                            x, y, z,
                            face,
                            textureLayer,
                            static_cast<uint8_t>(blockFacing)
                        });
                    } else {
                        const int baseVertexIndex = static_cast<int>(verticesOpaque.size()) / static_cast<int>(stride);
                        const int blockWorldX = coord.x * SIZE + x;
                        const int blockWorldY = coord.y * SIZE + y;
                        const int blockWorldZ = coord.z * SIZE + z;
                        const float light = TestFaceLightConstant();
                        for (int vertIdx = 0; vertIdx < 4; ++vertIdx) {
                            glm::vec3 worldCoord = glm::vec3(
                                blockWorldX,
                                blockWorldY,
                                blockWorldZ
                            ) + faceVerticesIndexed[face][vertIdx];
                            glm::vec2 faceUV = faceUVsIndexed[vertIdx];
                            if (face == 0 || face == 1)
                                faceUV = RotateTopBottomUV(faceUV, blockFacing);
                            float u = faceUV.x;
                            float v = 1.0f - faceUV.y;
                            ASCIIgL::VertStructs::PosUVLayerLight vertex = {};
                            vertex.SetXYZ(worldCoord);
                            vertex.SetUV(glm::vec2(u, v));
                            vertex.SetLayer(static_cast<float>(textureLayer));
                            vertex.SetLight(light);
                            const std::byte* vertexBytes = reinterpret_cast<const std::byte*>(&vertex);
                            verticesOpaque.insert(verticesOpaque.end(), vertexBytes, vertexBytes + sizeof(ASCIIgL::VertStructs::PosUVLayerLight));
                        }
                        for (int i = 0; i < 6; ++i)
                            indicesOpaque.push_back(baseVertexIndex + faceIndices[i]);
                    }
                }
            }
        }
    }

    if (!transparentFaces.empty()) {
        std::vector<size_t> order(transparentFaces.size());
        std::iota(order.begin(), order.end(), 0);
        const glm::vec3 D(0.0f, 0.0f, 1.0f);
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            const auto& fa = transparentFaces[a];
            const auto& fb = transparentFaces[b];
            float da = (coord.x * SIZE + fa.x + 0.5f) * D.x + (coord.y * SIZE + fa.y + 0.5f) * D.y + (coord.z * SIZE + fa.z + 0.5f) * D.z;
            float db = (coord.x * SIZE + fb.x + 0.5f) * D.x + (coord.y * SIZE + fb.y + 0.5f) * D.y + (coord.z * SIZE + fb.z + 0.5f) * D.z;
            return da > db;
        });

        for (size_t idx : order) {
            const auto& tf = transparentFaces[idx];
            int face = tf.faceIndex;
            int textureLayer = tf.textureLayer;
            BlockFace bf = static_cast<BlockFace>(tf.blockFacing);
            int baseVertexIndex = static_cast<int>(out.transparentVertices.size()) / static_cast<int>(stride);
            const int blockWorldX = coord.x * SIZE + tf.x;
            const int blockWorldY = coord.y * SIZE + tf.y;
            const int blockWorldZ = coord.z * SIZE + tf.z;
            const float light = TestFaceLightConstant();

            for (int vertIdx = 0; vertIdx < 4; ++vertIdx) {
                glm::vec3 worldCoord = glm::vec3(
                    blockWorldX,
                    blockWorldY,
                    blockWorldZ
                ) + faceVerticesIndexed[face][vertIdx];
                glm::vec2 faceUV = faceUVsIndexed[vertIdx];
                if (face == 0 || face == 1)
                    faceUV = RotateTopBottomUV(faceUV, bf);
                float u = faceUV.x;
                float v = 1.0f - faceUV.y;
                ASCIIgL::VertStructs::PosUVLayerLight vertex = {};
                vertex.SetXYZ(worldCoord);
                vertex.SetUV(glm::vec2(u, v));
                vertex.SetLayer(static_cast<float>(textureLayer));
                vertex.SetLight(light);
                const std::byte* vertexBytes = reinterpret_cast<const std::byte*>(&vertex);
                out.transparentVertices.insert(out.transparentVertices.end(), vertexBytes, vertexBytes + sizeof(ASCIIgL::VertStructs::PosUVLayerLight));
            }
            for (int i = 0; i < 6; ++i)
                out.transparentIndices.push_back(baseVertexIndex + faceIndices[i]);
        }
    }

    return out;
}
