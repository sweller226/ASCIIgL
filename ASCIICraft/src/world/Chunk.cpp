#include <ASCIICraft/world/Chunk.hpp>

#include <algorithm>
#include <cassert>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>



// Chunk constructor
Chunk::Chunk(const ChunkCoord& coord) 
    : coord(coord)
    , generated(false)
    , dirty(true)
    , hasMesh(false) {
    
    // Initialize all blocks to air (stateId 0)
    std::fill(blocks, blocks + VOLUME, blockstate::BlockStateRegistry::AIR_STATE_ID);
    
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

// Mesh generation
void Chunk::GenerateMesh(const blockstate::BlockStateRegistry& bsr) {
    if (!generated) {
        return; // Can't generate mesh for ungenerated chunk
    }

    std::vector<std::byte> vertices;
    std::vector<int> indices;

    // Get the block texture array
    auto blockTexturesPtr = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
    if (!blockTexturesPtr) {
        ASCIIgL::Logger::Warning("No texture array available for chunk mesh generation");
        return;
    }

    ASCIIgL::Logger::Debug("Generating mesh for chunk at (" + std::to_string(coord.x) + ", " + std::to_string(coord.y) + ", " + std::to_string(coord.z) + ")");
    
    ASCIIgL::TextureArray* blockTextures = blockTexturesPtr.get();

    // Face normal vectors for cube faces
    const glm::vec3 faceNormals[6] = {
        glm::vec3(0, 1, 0),   // Top
        glm::vec3(0, -1, 0),  // Bottom  
        glm::vec3(0, 0, 1),   // North (Front)
        glm::vec3(0, 0, -1),  // South (Back)
        glm::vec3(1, 0, 0),   // East (Right)
        glm::vec3(-1, 0, 0)   // West (Left)
    };
    
    // For indexed rendering, we need 4 unique vertices per face
    const glm::vec3 faceVerticesIndexed[6][4] = {
        // Top face (Y+) - 4 corners
        { glm::vec3(0, 1, 0), glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0) },
        // Bottom face (Y-)
        { glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(1, 0, 1) },
        // North face (Z+)
        { glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1) },
        // South face (Z-)
        { glm::vec3(1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(1, 1, 0) },
        // East face (X+)
        { glm::vec3(1, 0, 1), glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1) },
        // West face (X-)
        { glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(0, 1, 0) }
    };
    
    // UV coordinates for indexed rendering (4 corners)
    const glm::vec2 faceUVsIndexed[4] = {
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)
    };
    
    // Index pattern for each face (2 triangles)
    const int faceIndices[6] = { 0, 1, 2, 0, 2, 3 };
    
    // Iterate through all blocks in the chunk
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            for (int z = 0; z < SIZE; z++) {
                uint32_t stateId = GetBlockState(x, y, z);
                const auto& state = bsr.GetState(stateId);
                
                if (!state.isSolid) {
                    continue; // Skip air / non-solid blocks
                }
                
                // Check each face of the block
                for (int face = 0; face < 6; face++) {
                    bool shouldRenderFace = false;
                    
                    // Check if face should be rendered (neighbor is air or at chunk boundary)
                    int neighborX = x, neighborY = y, neighborZ = z;
                    
                    switch (face) {
                        case 0: neighborY++; break; // Top
                        case 1: neighborY--; break; // Bottom
                        case 2: neighborZ++; break; // North
                        case 3: neighborZ--; break; // South
                        case 4: neighborX++; break; // East
                        case 5: neighborX--; break; // West
                    }
                    
                    // Check if neighbor is air (either in this chunk or neighboring chunk)
                    if (IsValidBlockCoord(neighborX, neighborY, neighborZ)) {
                        // Neighbor is within this chunk
                        uint32_t neighborStateId = GetBlockState(neighborX, neighborY, neighborZ);
                        shouldRenderFace = !bsr.GetState(neighborStateId).isSolid;
                    } else {
                        // Neighbor is in adjacent chunk - check neighboring chunk for face culling
                        shouldRenderFace = true; // Default to render if we can't determine neighbor state
                        
                        // Determine which neighbor chunk to check and the local coordinates within it
                        int neighborChunkDir = -1;
                        int localX = x, localY = y, localZ = z; // Start with current block coords
                        
                        if (neighborX < 0) {
                            neighborChunkDir = 5; // West neighbor
                            localX = SIZE - 1;   // Rightmost edge of west chunk
                        } else if (neighborX >= SIZE) {
                            neighborChunkDir = 4; // East neighbor
                            localX = 0;          // Leftmost edge of east chunk
                        } else if (neighborY < 0) {
                            neighborChunkDir = 1; // Bottom neighbor
                            localY = SIZE - 1;   // Top edge of bottom chunk
                        } else if (neighborY >= SIZE) {
                            neighborChunkDir = 0; // Top neighbor
                            localY = 0;          // Bottom edge of top chunk
                        } else if (neighborZ < 0) {
                            neighborChunkDir = 3; // South neighbor
                            localZ = SIZE - 1;   // Front edge of south chunk
                        } else if (neighborZ >= SIZE) {
                            neighborChunkDir = 2; // North neighbor
                            localZ = 0;          // Back edge of north chunk
                        }
                        
                        // Check the neighboring chunk if it exists and is generated
                        if (neighborChunkDir >= 0) {
                            Chunk* neighborChunk = GetNeighbor(neighborChunkDir);
                            // CRITICAL: Check for null pointer before accessing neighbor
                            if (neighborChunk != nullptr && neighborChunk->IsGenerated()) {
                                // Check if the neighbor block in adjacent chunk is solid
                                uint32_t neighborStateId = neighborChunk->GetBlockState(localX, localY, localZ);
                                shouldRenderFace = !bsr.GetState(neighborStateId).isSolid;
                            }
                            // If neighbor is null or not generated, render the face (conservative approach)
                        }
                    }
                    
                    if (!shouldRenderFace) {
                        continue;
                    }
                    
                    // Get texture layer index for this block face from blockstate
                    int textureLayer = state.faceTextureLayers[face];
                    
                    // Indexed rendering: generate 4 unique vertices
                    int baseVertexIndex = vertices.size() / ASCIIgL::VertFormats::PosUVLayer().GetStride();
                    
                    for (int vertIdx = 0; vertIdx < 4; vertIdx++) {
                        // World position (chunk-relative + block position)
                        glm::vec3 WorldCoord = glm::vec3(
                            coord.x * SIZE + x,
                            coord.y * SIZE + y, 
                            coord.z * SIZE + z
                        ) + faceVerticesIndexed[face][vertIdx];
                        
                        // Simple 0-1 UVs + layer index for Texture2DArray
                        glm::vec2 faceUV = faceUVsIndexed[vertIdx];
                        float u = faceUV.x;
                        float v = 1.0f - faceUV.y;
                        
                        ASCIIgL::VertStructs::PosUVLayer vertex = {
                            WorldCoord.x, WorldCoord.y, WorldCoord.z,  // XYZ
                            u, v, static_cast<float>(textureLayer)     // UV + Layer
                        };
                        
                        // Append bytes to vertices vector
                        const auto* vertexBytes = reinterpret_cast<const std::byte*>(&vertex);
                        vertices.insert(vertices.end(), vertexBytes, vertexBytes + sizeof(ASCIIgL::VertStructs::PosUVLayer));
                    }
                    
                    // Add indices for this face (2 triangles)
                    for (int i = 0; i < 6; i++) {
                        indices.push_back(baseVertexIndex + faceIndices[i]);
                    }
                }
            }
        }
    }
    
    // Create the mesh
    if (!vertices.empty()) {
        // Validate texture array
        if (!blockTextures || !blockTextures->IsValid()) {
            ASCIIgL::Logger::Error("  Block texture array is invalid!");
            mesh.reset();
            hasMesh = false;
            SetDirty(false);
            return;
        }
        
        if (!indices.empty()) {
            // Use the new constructor that accepts TextureArray*
            mesh = std::make_unique<ASCIIgL::Mesh>(std::move(vertices), ASCIIgL::VertFormats::PosUVLayer(), std::move(indices), blockTextures);
            ASCIIgL::Logger::Debug("Indexed mesh created successfully with " + std::to_string(vertices.size()) + " bytes and " + std::to_string(indices.size()) + " indices");
        }

        hasMesh = true;
    } else {
        mesh.reset();
        hasMesh = false;
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

void Chunk::LogNeighbors() const {
    for (int i = 0; i < 6; ++i) {
        if (neighbors[i]) {
            ASCIIgL::Logger::Debug("Neighbor " + std::to_string(i) + ": " + neighbors[i]->coord.ToString());
        } else {
            ASCIIgL::Logger::Debug("Neighbor " + std::to_string(i) + ": nullptr");
        }
    }
}

void Chunk::Render() const {
    if (!hasMesh || !mesh) {
        return;
    }
    
    // Use DrawMesh instead of batching - leverages GPU mesh caching
    ASCIIgL::Renderer::GetInst().DrawMesh(mesh.get());
}