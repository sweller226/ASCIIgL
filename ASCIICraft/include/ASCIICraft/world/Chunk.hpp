#pragma once

#include <unordered_map>
#include <memory>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/glm.hpp>

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Coords.hpp>

// Chunk class - contains 16x16x16 blocks
class Chunk {
public:
    static constexpr int SIZE = 16;
    static constexpr int VOLUME = SIZE * SIZE * SIZE;
    
    Chunk(const ChunkCoord& coord);
    ~Chunk() = default;
    
    // Block access
    Block& GetBlock(int x, int y, int z);
    const Block& GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, const Block& block);
    
    // Chunk properties
    const ChunkCoord& GetCoord() const { return coord; }
    bool IsGenerated() const { return generated; }
    bool IsDirty() const { return dirty; }
    void SetDirty(bool d) { dirty = d; }
    void SetGenerated(bool g) { generated = g; }
    
    // Mesh generation for rendering
    void GenerateMesh();
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

    Block& GetBlockByIndex(int i);
    const Block& GetBlockByIndex(int i) const;

    void SetBlockByIndex(int i, Block& block);
    
private:
    ChunkCoord coord;
    Block blocks[VOLUME];  // 16x16x16 = 4096 blocks
    
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