#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/VertexShaderCPU.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>

// Forward declarations
struct ChunkCoord;
struct WorldPos;

// Chunk coordinates (world space divided by chunk size)
struct ChunkCoord {
    int x, y, z;
    
    ChunkCoord() : x(0), y(0), z(0) {}
    ChunkCoord(int x, int y, int z) : x(x), y(y), z(z) {}
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const ChunkCoord& other) const {
        return !(*this == other);
    }

    ChunkCoord operator+(const ChunkCoord& other) const {
        return ChunkCoord(x + other.x, y + other.y, z + other.z);
    }

    ChunkCoord operator-(const ChunkCoord& other) const {
        return ChunkCoord(x - other.x, y - other.y, z - other.z);
    }

    std::string ToString() const {
        return "(" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")";
    }
};

// World position (block coordinates)
struct WorldPos {
    int x, y, z;
    
    WorldPos() : x(0), y(0), z(0) {}
    WorldPos(int x, int y, int z) : x(x), y(y), z(z) {}
    WorldPos(const glm::vec3& pos) : x(static_cast<int>(pos.x)), y(static_cast<int>(pos.y)), z(static_cast<int>(pos.z)) {}
    
    glm::vec3 ToVec3() const {
        return glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    
    ChunkCoord ToChunkCoord() const {
        constexpr int CHUNK_SIZE = 16;
        return ChunkCoord(
            x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE,
            y >= 0 ? y / CHUNK_SIZE : (y - CHUNK_SIZE + 1) / CHUNK_SIZE,
            z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE
        );
    }
};

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
    WorldPos ChunkToWorldPos(int x, int y, int z) const;

    // Logging
    void LogNeighbors() const;
    
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

// Hash function specialization for ChunkCoord (needed for unordered_map)
namespace std {
    template<>
    struct hash<ChunkCoord> {
        size_t operator()(const ChunkCoord& coord) const {
            // Combine the hash of x, y, z coordinates
            size_t h1 = hash<int>()(coord.x);
            size_t h2 = hash<int>()(coord.y);
            size_t h3 = hash<int>()(coord.z);
            
            // Use a simple hash combination technique
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}
