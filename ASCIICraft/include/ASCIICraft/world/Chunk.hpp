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
    bool HasMesh() const { return hasMesh; }
    ASCIIgL::Mesh* GetMesh() const { return mesh.get(); }
    void InvalidateMesh() { hasMesh = false; dirty = true; }
    
    // Rendering
    void Render() const;
    
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
    bool hasMesh;
    
    // Mesh data for rendering
    std::unique_ptr<ASCIIgL::Mesh> mesh;
    
    // Neighbor chunks (6 directions: +X, -X, +Y, -Y, +Z, -Z)
    Chunk* neighbors[6];
    
    // Helper methods
    int GetBlockIndex(int x, int y, int z) const;
};