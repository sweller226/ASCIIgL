#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/ChunkMeshGen.hpp>

#include <algorithm>
#include <cassert>
#include <array>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>



// Chunk constructor
Chunk::Chunk(const ChunkCoord& coord) 
    : coord(coord)
    , generated(false)
    , dirty(true)
    , hasOpaqueMesh(false) {
    
    // Initialize all blocks to air (stateId 0)
    std::fill(blocks, blocks + VOLUME, blockstate::BlockStateRegistry::AIR_STATE_ID);
    
    // Initialize transparent mesh flag
    hasTransparentMesh = false;

    // Initialize neighbor pointers to nullptr
    for (int i = 0; i < 6; ++i) {
        neighbors[i] = nullptr;
    }
}

// Block access methods
uint32_t Chunk::GetBlockState(int x, int y, int z) const {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    return blocks[GetBlockIndex(x, y, z)];
}

void Chunk::SetBlockState(int x, int y, int z, uint32_t stateId) {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    
    int index = GetBlockIndex(x, y, z);
    if (blocks[index] != stateId) {
        blocks[index] = stateId;
        InvalidateMesh();
    }
}

uint32_t Chunk::GetBlockStateByIndex(int i) const {
    if (0 <= i && i < VOLUME) { return blocks[i]; }
    ASCIIgL::Logger::Warning("GetBlockStateByIndex: index out of bounds");
    return blockstate::BlockStateRegistry::AIR_STATE_ID;
}

void Chunk::SetBlockStateByIndex(int i, uint32_t stateId) {
    if (0 <= i && i < VOLUME) { blocks[i] = stateId; }
}

    // Mesh generation (uses shared BuildChunkMeshData from ChunkMeshGen)
void Chunk::GenerateMesh(const blockstate::BlockStateRegistry& bsr) {
    if (!generated) return;

    auto blockTexturesPtr = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
    if (!blockTexturesPtr || !blockTexturesPtr->IsValid()) {
        ASCIIgL::Logger::Warning("No texture array available for chunk mesh generation");
        return;
    }
    ASCIIgL::TextureArray* blockTextures = blockTexturesPtr.get();

    // Copy block data for shared mesh builder (chunk + neighbors)
    std::vector<uint32_t> chunkBlocksCopy(VOLUME);
    for (int i = 0; i < VOLUME; ++i)
        chunkBlocksCopy[i] = GetBlockStateByIndex(i);

    std::array<std::vector<uint32_t>, 6> neighborCopies;
    std::array<const uint32_t*, 6> neighborBlocks{};
    for (int i = 0; i < 6; ++i) {
        Chunk* n = GetNeighbor(i);
        if (n && n->IsGenerated()) {
            neighborCopies[i].resize(VOLUME);
            for (int j = 0; j < VOLUME; ++j)
                neighborCopies[i][j] = n->GetBlockStateByIndex(j);
            neighborBlocks[i] = neighborCopies[i].data();
        } else {
            neighborBlocks[i] = nullptr;
        }
    }

    ChunkMeshData meshData = BuildChunkMeshData(coord, chunkBlocksCopy.data(), neighborBlocks, &bsr);
    ApplyMeshData(std::move(meshData), blockTextures);
}

void Chunk::ApplyMeshData(ChunkMeshData&& data, ASCIIgL::TextureArray* blockTextures) {
    if (!blockTextures || !blockTextures->IsValid()) return;

    opaqueMesh.reset();
    transparentMesh.reset();
    hasOpaqueMesh = false;
    hasTransparentMesh = false;

    if (data.HasOpaque()) {
        opaqueMesh = std::make_unique<ASCIIgL::Mesh>(
            std::move(data.opaqueVertices),
            ASCIIgL::VertFormats::PosUVLayerLight(),
            std::move(data.opaqueIndices),
            blockTextures
        );
        hasOpaqueMesh = (opaqueMesh != nullptr);
    }

    if (data.HasTransparent()) {
        transparentMesh = std::make_unique<ASCIIgL::Mesh>(
            std::move(data.transparentVertices),
            ASCIIgL::VertFormats::PosUVLayerLight(),
            std::move(data.transparentIndices),
            blockTextures
        );
        hasTransparentMesh = (transparentMesh != nullptr);
    }

    SetDirty(false);
}

// Neighbor management
void Chunk::SetNeighbor(int direction, Chunk* neighbor) {
    assert(direction >= 0 && direction < 6 && "Invalid neighbor direction");
    neighbors[direction] = neighbor;
}

Chunk* Chunk::GetNeighbor(int direction) const {
    assert(direction >= 0 && direction < 6 && "Invalid neighbor direction");
    return neighbors[direction];
}

// Utility methods
bool Chunk::IsValidBlockCoord(int x, int y, int z) const {
    return x >= 0 && x < SIZE && 
           y >= 0 && y < SIZE && 
           z >= 0 && z < SIZE;
}

WorldCoord Chunk::ChunkToWorldCoord(int x, int y, int z) const {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    
    return WorldCoord(
        coord.x * SIZE + x,
        coord.y * SIZE + y,
        coord.z * SIZE + z
    );
}

// Helper methods
int Chunk::GetBlockIndex(int x, int y, int z) const {
    // 3D array indexing: index = x + y*SIZE + z*SIZE*SIZE
    return x + y * SIZE + z * SIZE * SIZE;
}

// Rendering is now handled by ChunkManager via Renderer draw-call queue.