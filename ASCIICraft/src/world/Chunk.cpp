#include <ASCIICraft/world/Chunk.hpp>

#include <algorithm>
#include <cassert>
#include <array>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>

#include <ASCIICraft/world/ChunkUtil.hpp>

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
    assert(chunkutil::IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    return blocks[chunkutil::GetBlockIndex(x, y, z)];
}

void Chunk::SetBlockState(int x, int y, int z, uint32_t stateId) {
    assert(chunkutil::IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    
    int index = chunkutil::GetBlockIndex(x, y, z);
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

WorldCoord Chunk::ChunkToWorldCoord(int x, int y, int z) const {
    assert(chunkutil::IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    
    return WorldCoord(
        coord.x * sizes::CHUNK_SIZE + x,
        coord.y * sizes::CHUNK_SIZE + y,
        coord.z * sizes::CHUNK_SIZE + z
    );
}

// Rendering is now handled by ChunkManager via Renderer draw-call queue.