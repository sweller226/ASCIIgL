#include <ASCIICraft/world/Chunk.hpp>

#include <algorithm>
#include <cassert>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/VertexShader.hpp>
#include <ASCIIgL/engine/Logger.hpp>

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
    
    std::vector<VERTEX> vertices;
    std::vector<Texture*> textures;
    
    // Get the block texture atlas
    if (!Block::HasTextureAtlas()) {
        Logger::Warning("No texture atlas available for chunk mesh generation");
        return;
    }
    
    Texture* blockAtlas = Block::GetTextureAtlas();
    textures.push_back(blockAtlas);
    
    // Face normal vectors for cube faces
    const glm::vec3 faceNormals[6] = {
        glm::vec3(0, 1, 0),   // Top
        glm::vec3(0, -1, 0),  // Bottom  
        glm::vec3(0, 0, 1),   // North (Front)
        glm::vec3(0, 0, -1),  // South (Back)
        glm::vec3(1, 0, 0),   // East (Right)
        glm::vec3(-1, 0, 0)   // West (Left)
    };
    
    // Face vertex offsets for cube (each face is 2 triangles = 6 vertices)
    const glm::vec3 faceVertices[6][6] = {
        // Top face (Y+)
        {
            glm::vec3(0, 1, 0), glm::vec3(1, 1, 0), glm::vec3(1, 1, 1),
            glm::vec3(0, 1, 0), glm::vec3(1, 1, 1), glm::vec3(0, 1, 1)
        },
        // Bottom face (Y-)
        {
            glm::vec3(0, 0, 1), glm::vec3(1, 0, 1), glm::vec3(1, 0, 0),
            glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 0, 0)
        },
        // North face (Z+)
        {
            glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), glm::vec3(1, 1, 1),
            glm::vec3(0, 0, 1), glm::vec3(1, 1, 1), glm::vec3(1, 0, 1)
        },
        // South face (Z-)
        {
            glm::vec3(1, 0, 0), glm::vec3(1, 1, 0), glm::vec3(0, 1, 0),
            glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 0)
        },
        // East face (X+)
        {
            glm::vec3(1, 0, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 0),
            glm::vec3(1, 0, 1), glm::vec3(1, 1, 0), glm::vec3(1, 0, 0)
        },
        // West face (X-)
        {
            glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 1),
            glm::vec3(0, 0, 0), glm::vec3(0, 1, 1), glm::vec3(0, 0, 1)
        }
    };
    
    // UV coordinates for face (2 triangles = 6 vertices)
    const glm::vec2 faceUVs[6] = {
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1),
        glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(0, 1)
    };
    
    // Iterate through all blocks in the chunk
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            for (int z = 0; z < SIZE; z++) {
                const Block& block = GetBlock(x, y, z);
                
                if (!block.IsSolid()) {
                    continue; // Skip air blocks
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
                        shouldRenderFace = !GetBlock(neighborX, neighborY, neighborZ).IsSolid();
                    } else {
                        // Neighbor is in adjacent chunk - for now, always render border faces
                        // TODO: Implement cross-chunk face culling using neighbor chunks
                        shouldRenderFace = true;
                    }
                    
                    if (!shouldRenderFace) {
                        continue;
                    }
                    
                    // Get texture coordinates for this block face
                    glm::vec4 blockTextureUV = block.GetTextureUV(face);
                    
                    // Generate vertices for this face
                    for (int vertIdx = 0; vertIdx < 6; vertIdx++) {
                        VERTEX vertex;
                        
                        // World position (chunk-relative + block position)
                        glm::vec3 worldPos = glm::vec3(
                            coord.x * SIZE + x,
                            coord.y * SIZE + y, 
                            coord.z * SIZE + z
                        ) + faceVertices[face][vertIdx];
                        
                        vertex.SetXYZW(glm::vec4(worldPos, 1.0f));
                        
                        // UV coordinates (map face UV to block's texture UV)
                        glm::vec2 faceUV = faceUVs[vertIdx];
                        glm::vec2 textureUV = glm::vec2(
                            blockTextureUV.x + faceUV.x * (blockTextureUV.z - blockTextureUV.x),
                            blockTextureUV.y + faceUV.y * (blockTextureUV.w - blockTextureUV.y)
                        );
                        vertex.SetUV(textureUV);
                        
                        // Normal vector
                        vertex.SetNXYZ(faceNormals[face]);
                        
                        vertices.push_back(vertex);
                    }
                }
            }
        }
    }
    
    // Create the mesh
    if (!vertices.empty()) {
        mesh = std::make_unique<Mesh>(std::move(vertices), std::move(textures));
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

void Chunk::Render(VERTEX_SHADER& vertex_shader, const Camera3D& camera) {
    if (!HasMesh() || !mesh) {
        return; // No mesh to render
    }

    Renderer::DrawMesh(vertex_shader, mesh.get(), camera);
}