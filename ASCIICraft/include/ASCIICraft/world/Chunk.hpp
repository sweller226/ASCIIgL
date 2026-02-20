#pragma once

#include <unordered_map>
#include <memory>
#include <cstdint>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <ASCIICraft/world/Coords.hpp>

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
    
    // Chunk properties
    const ChunkCoord& GetCoord() const { return coord; }
    bool IsGenerated() const { return generated; }
    bool IsDirty() const { return dirty; }
    void SetDirty(bool d) { dirty = d; }
    void SetGenerated(bool g) { generated = g; }
    
    // Mesh generation for rendering (needs registry for texture/solidity lookups)
    void GenerateMesh(const blockstate::BlockStateRegistry& bsr);

    // Mesh access
    bool HasOpaqueMesh() const { return hasOpaqueMesh; }
    bool HasTransparentMesh() const {
        for (bool h : hasTransparentMesh) {
            if (h) return true;
        }
        return false;
    }
    bool HasMesh() const { return HasOpaqueMesh() || HasTransparentMesh(); }

    ASCIIgL::Mesh* GetOpaqueMesh() const { return opaqueMesh.get(); }

    // Transparent meshes are pre-baked for multiple view directions (variants).
    static constexpr int TRANSPARENT_VARIANT_COUNT = 6; // +X, -X, +Y, -Y, +Z, -Z

    bool HasTransparentMeshVariant(int idx) const {
        return (idx >= 0 && idx < TRANSPARENT_VARIANT_COUNT) ? hasTransparentMesh[idx] : false;
    }

    ASCIIgL::Mesh* GetTransparentMeshVariant(int idx) const {
        return (idx >= 0 && idx < TRANSPARENT_VARIANT_COUNT) ? transparentMeshes[idx].get() : nullptr;
    }

    void InvalidateMesh() {
        hasOpaqueMesh = false;
        opaqueMesh.reset();
        for (int i = 0; i < TRANSPARENT_VARIANT_COUNT; ++i) {
            hasTransparentMesh[i] = false;
            transparentMeshes[i].reset();
        }
        dirty = true;
    }
    
    // Neighbor access for mesh generation
    void SetNeighbor(int direction, Chunk* neighbor);
    Chunk* GetNeighbor(int direction) const;
    
    // Utility
    bool IsValidBlockCoord(int x, int y, int z) const;
    WorldCoord ChunkToWorldCoord(int x, int y, int z) const;

    // Logging
    void LogNeighbors() const;
    
private:
    ChunkCoord coord;
    uint32_t blocks[VOLUME];  // blockstate IDs, 16x16x16 = 4096 entries
    
    bool generated;
    bool dirty;

    // Mesh data for rendering (split into opaque and multiple transparent variants)
    bool hasOpaqueMesh;
    bool hasTransparentMesh[TRANSPARENT_VARIANT_COUNT];

    std::unique_ptr<ASCIIgL::Mesh> opaqueMesh;
    std::unique_ptr<ASCIIgL::Mesh> transparentMeshes[TRANSPARENT_VARIANT_COUNT];
    
    // Neighbor chunks (6 directions: +X, -X, +Y, -Y, +Z, -Z)
    Chunk* neighbors[6];
    
    // Helper methods
    int GetBlockIndex(int x, int y, int z) const;
};