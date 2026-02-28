#pragma once

#include <unordered_map>
#include <memory>
#include <cstdint>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/ChunkMeshGen.hpp>

namespace ASCIIgL { class TextureArray; }

// Chunk class - contains 16x16x16 blocks stored as blockstate IDs
class Chunk {
public:
    static constexpr int SIZE = 16;
    static constexpr int VOLUME = SIZE * SIZE * SIZE;
    
    Chunk(const ChunkCoord& coord);
    ~Chunk() = default;
    
    // Block access (blockstate IDs)
    uint32_t GetBlockState(int x, int y, int z) const;
    void SetBlockState(int x, int y, int z, uint32_t stateId);
    
    uint32_t GetBlockStateByIndex(int i) const;
    void SetBlockStateByIndex(int i, uint32_t stateId);
    /// Contiguous block data for bulk copy (e.g. mesh/terrain jobs). Size Chunk::VOLUME.
    const uint32_t* GetBlockData() const { return blocks; }

    // Chunk properties
    const ChunkCoord& GetCoord() const { return coord; }
    bool IsGenerated() const { return generated; }
    bool IsDirty() const { return dirty; }
    void SetDirty(bool d) { dirty = d; }
    void SetGenerated(bool g) { generated = g; }
    
    // Mesh generation for rendering (needs registry for texture/solidity lookups)
    void GenerateMesh(const blockstate::BlockStateRegistry& bsr);

    /// Apply mesh data from job queue (creates Mesh objects and assigns). Call on main thread only.
    void ApplyMeshData(ChunkMeshData&& data, ASCIIgL::TextureArray* blockTextures);

    /// Apply terrain result from job queue (block data). Call on main thread only. Sets generated when count == VOLUME.
    void ApplyBlockData(const uint32_t* blocks, size_t count);

    // Mesh access
    bool HasOpaqueMesh() const { return hasOpaqueMesh; }
    bool HasTransparentMesh() const { return hasTransparentMesh; }
    bool HasMesh() const { return HasOpaqueMesh() || HasTransparentMesh(); }

    ASCIIgL::Mesh* GetOpaqueMesh() const { return opaqueMesh.get(); }

    ASCIIgL::Mesh* GetTransparentMesh() const { return transparentMesh.get(); }

    void InvalidateMesh() {
        hasOpaqueMesh = false;
        opaqueMesh.reset();
        hasTransparentMesh = false;
        transparentMesh.reset();
        dirty = true;
    }
    
    // Neighbor access for mesh generation
    void SetNeighbor(int direction, Chunk* neighbor);
    Chunk* GetNeighbor(int direction) const;
    
    // Utility
    bool IsValidBlockCoord(int x, int y, int z) const;
    WorldCoord ChunkToWorldCoord(int x, int y, int z) const;
    
private:
    ChunkCoord coord;
    uint32_t blocks[VOLUME];  // blockstate IDs, 16x16x16 = 4096 entries
    
    bool generated;
    bool dirty;

    // Mesh data for rendering (split into opaque and transparent)
    bool hasOpaqueMesh;
    bool hasTransparentMesh;

    std::unique_ptr<ASCIIgL::Mesh> opaqueMesh;
    std::unique_ptr<ASCIIgL::Mesh> transparentMesh;
    
    // Neighbor chunks (6 directions: +X, -X, +Y, -Y, +Z, -Z)
    Chunk* neighbors[6];
    
    // Helper methods
    int GetBlockIndex(int x, int y, int z) const;
};