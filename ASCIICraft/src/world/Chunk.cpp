#include <ASCIICraft/world/Chunk.hpp>
#include <algorithm>
#include <cassert>

// Chunk constructor
Chunk::Chunk(const ChunkCoord& coord) 
    : coord(coord)
    , generated(false)
    , dirty(true)
    , hasMesh(false) {
    
    // Initialize all blocks to air
    std::fill(blocks, blocks + VOLUME, Block(BlockType::Air));
    
    // Initialize neighbor pointers to nullptr
    for (int i = 0; i < 6; ++i) {
        neighbors[i] = nullptr;
    }
}

// Block access methods
Block& Chunk::GetBlock(int x, int y, int z) {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    return blocks[GetBlockIndex(x, y, z)];
}

const Block& Chunk::GetBlock(int x, int y, int z) const {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    return blocks[GetBlockIndex(x, y, z)];
}

void Chunk::SetBlock(int x, int y, int z, const Block& block) {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    
    int index = GetBlockIndex(x, y, z);
    if (blocks[index].type != block.type || blocks[index].metadata != block.metadata) {
        blocks[index] = block;
        SetDirty(true);
        InvalidateMesh();
    }
}

// Mesh generation
void Chunk::GenerateMesh() {
    if (!generated) {
        return; // Can't generate mesh for ungenerated chunk
    }
    
    // TODO: Implement actual mesh generation for rendering
    // This would typically:
    // 1. Iterate through all blocks in the chunk
    // 2. For each solid block, check which faces are visible
    // 3. Generate vertices and triangles for visible faces
    // 4. Store the mesh data for rendering
    
    hasMesh = true;
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

WorldPos Chunk::ChunkToWorldPos(int x, int y, int z) const {
    assert(IsValidBlockCoord(x, y, z) && "Block coordinates out of range");
    
    return WorldPos(
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
