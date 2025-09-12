#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/player/Player.hpp>

#include <algorithm>
#include <cmath>

#include <ASCIIgL/engine/Logger.hpp>

// Static member definition
const ChunkCoord World::NEIGHBOR_OFFSETS[6] = {
    ChunkCoord(1, 0, 0),   // NEIGHBOR_POS_X
    ChunkCoord(-1, 0, 0),  // NEIGHBOR_NEG_X
    ChunkCoord(0, 1, 0),   // NEIGHBOR_POS_Y
    ChunkCoord(0, -1, 0),  // NEIGHBOR_NEG_Y
    ChunkCoord(0, 0, 1),   // NEIGHBOR_POS_Z
    ChunkCoord(0, 0, -1)   // NEIGHBOR_NEG_Z
};

World::World(unsigned int renderDistance, const WorldPos& spawnPoint) 
    : renderDistance(renderDistance)
    , spawnPoint(spawnPoint)
    , player(nullptr) {
    Logger::Info("World created");
}

World::~World() {
    Logger::Info("World destroyed");
}

void World::Update() {
    // Update chunk loading based on player position
    if (player) {
        UpdateChunkLoading();
    }
    
    // Regenerate dirty chunks in batches to prevent chain reactions
    RegenerateDirtyChunks();
}

void World::Render() {
    if (!player) {
        Logger::Warning("No player set for world rendering");
        return;
    }

    glm::vec3 playerPos = player->GetPosition();
    glm::vec3 viewDir = player->GetCamera().getCamFront();
    
    Logger::Debug("World::Render - Player position: (" + std::to_string(playerPos.x) + ", " + 
                  std::to_string(playerPos.y) + ", " + std::to_string(playerPos.z) + ")");

    std::vector<Chunk*> visibleChunks = GetVisibleChunks(playerPos, viewDir);
    Logger::Debug("World::Render - Found " + std::to_string(visibleChunks.size()) + " visible chunks");
    
    int renderedChunks = 0;
    for (Chunk* chunk : visibleChunks) {
        if (chunk->HasMesh()) {
            chunk->Render(vertex_shader, player->GetCamera());
            renderedChunks++;
        }
    }
    
    Logger::Debug("World::Render - Rendered " + std::to_string(renderedChunks) + " chunks with meshes");
}

void World::GenerateWorld() {
    // Generate chunks in the surrounding area (cubic)
    for (int x = -renderDistance; x <= renderDistance; ++x) {
        for (int y = -renderDistance; y <= renderDistance; ++y) {  // Now includes Y-axis
            for (int z = -renderDistance; z <= renderDistance; ++z) {
                ChunkCoord chunkCoord(x, y, z);  // Full 3D chunk generation
                GetOrCreateChunk(chunkCoord);
            }
        }
    }
}

Block World::GetBlock(const WorldPos& pos) {
    return GetBlock(pos.x, pos.y, pos.z);
}

Block World::GetBlock(int x, int y, int z) {
    ChunkCoord chunkCoord = WorldPosToChunkCoord(WorldPos(x, y, z));
    
    Chunk* chunk = GetChunk(chunkCoord);
    if (!chunk) {
        return Block(); // Return air block if chunk not loaded
    }
    
    glm::ivec3 localPos = WorldPosToLocalChunkPos(WorldPos(x, y, z));
    return chunk->GetBlock(localPos.x, localPos.y, localPos.z);
}

void World::SetBlock(const WorldPos& pos, const Block& block) {
    SetBlock(pos.x, pos.y, pos.z, block);
}

void World::SetBlock(int x, int y, int z, const Block& block) {
    ChunkCoord chunkCoord = WorldPosToChunkCoord(WorldPos(x, y, z));
    
    Chunk* chunk = GetOrCreateChunk(chunkCoord);
    if (!chunk) {
        Logger::Error("Failed to get or create chunk for block placement");
        return;
    }
    
    glm::ivec3 localPos = WorldPosToLocalChunkPos(WorldPos(x, y, z));
    chunk->SetBlock(localPos.x, localPos.y, localPos.z, block);
    
    // Batch invalidate meshes to prevent chain reactions
    BatchInvalidateChunkMeshes(chunkCoord);
}

Chunk* World::GetChunk(const ChunkCoord& coord) {
    auto it = loadedChunks.find(coord);
    if (it != loadedChunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk* World::GetOrCreateChunk(const ChunkCoord& coord) {
    Chunk* chunk = GetChunk(coord);
    if (!chunk) {
        LoadChunk(coord);
        chunk = GetChunk(coord);
    }
    return chunk;
}

void World::LoadChunk(const ChunkCoord& coord) {
    if (IsChunkLoaded(coord)) {
        return; // Already loaded
    }
    
    // Create new chunk
    auto chunk = std::make_unique<Chunk>(coord);
    
    // Store the chunk first so GetChunk() works
    loadedChunks[coord] = std::move(chunk);
    
    // Now generate terrain (GetChunk will find the stored chunk)
    GenerateEmptyChunk(coord);
    
    // Update neighbors
    UpdateChunkNeighbors(coord);
    
    Logger::Debug("Loaded chunk at (" + std::to_string(coord.x) + ", " + 
                  std::to_string(coord.y) + ", " + std::to_string(coord.z) + ")");
}

void World::UnloadChunk(const ChunkCoord& coord) {
    auto it = loadedChunks.find(coord);
    if (it != loadedChunks.end()) {
        loadedChunks.erase(it);
        Logger::Debug("Unloaded chunk at (" + std::to_string(coord.x) + ", " + 
                      std::to_string(coord.y) + ", " + std::to_string(coord.z) + ")");
    }
}

bool World::IsChunkLoaded(const ChunkCoord& coord) const {
    return loadedChunks.find(coord) != loadedChunks.end();
}

void World::UpdateChunkLoading() {
    if (!player) {
        return;
    }
    
    // Get player chunk position
    glm::vec3 playerPos = player->GetPosition();
    ChunkCoord playerChunk = WorldPosToChunkCoord(WorldPos(playerPos));
    
    // Get chunks that should be loaded
    std::vector<ChunkCoord> chunksToLoad = GetChunksInRadius(playerChunk, renderDistance);
    
    // Load new chunks
    for (const ChunkCoord& coord : chunksToLoad) {
        if (!IsChunkLoaded(coord)) {
            LoadChunk(coord);
        }
    }
    
    // Unload distant chunks
    std::vector<ChunkCoord> chunksToUnload;
    for (const auto& pair : loadedChunks) {
        const ChunkCoord& coord = pair.first;
        int dx = coord.x - playerChunk.x;
        int dy = coord.y - playerChunk.y;
        int dz = coord.z - playerChunk.z;
        int distance = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
        
        if (distance > renderDistance + 1) { // +1 for hysteresis
            chunksToUnload.push_back(coord);
        }
    }
    
    for (const ChunkCoord& coord : chunksToUnload) {
        UnloadChunk(coord);
    }
}

void World::GenerateEmptyChunk(const ChunkCoord& coord) {
    // For now, just generate an empty chunk
    // TODO: Implement proper terrain generation
    Chunk* chunk = GetChunk(coord);
    if (chunk) {
        // Generate a simple test pattern
        if (coord.y == 0) {
            // Generate grass layer at y=0 chunks
            for (int x = 0; x < Chunk::SIZE; ++x) {
                for (int z = 0; z < Chunk::SIZE; ++z) {
                    chunk->SetBlock(x, 0, z, Block(BlockType::Grass));
                }
            }
        }
        
        // Mark chunk as generated and generate its mesh
        chunk->SetGenerated(true);
        chunk->GenerateMesh();
    }
}

std::vector<Chunk*> World::GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const {
    std::vector<Chunk*> visibleChunks;
    
    // Simple frustum culling - for now just return all loaded chunks within render distance
    ChunkCoord playerChunk = WorldPosToChunkCoord(WorldPos(playerPos));
    
    for (const auto& pair : loadedChunks) {
        const ChunkCoord& coord = pair.first;
        Chunk* chunk = pair.second.get();
        
        int dx = coord.x - playerChunk.x;
        int dy = coord.y - playerChunk.y;
        int dz = coord.z - playerChunk.z;
        int distance = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
        
        if (distance <= renderDistance) {
            visibleChunks.push_back(chunk);
        }
    }
    
    return visibleChunks;
}

void World::BatchInvalidateChunkMeshes(const ChunkCoord& coord) {
    // Collect all chunks that need invalidation without triggering regeneration
    std::unordered_set<ChunkCoord> chunksToInvalidate;
    
    // Add the main chunk
    chunksToInvalidate.insert(coord);
    
    // Add neighboring chunks
    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + NEIGHBOR_OFFSETS[i];
        if (IsChunkLoaded(neighborCoord)) {
            chunksToInvalidate.insert(neighborCoord);
        }
    }
    
    // Invalidate all collected chunks at once (mark as dirty, don't regenerate yet)
    for (const ChunkCoord& chunkCoord : chunksToInvalidate) {
        Chunk* chunk = GetChunk(chunkCoord);
        if (chunk) {
            chunk->InvalidateMesh();
        }
    }
    
    Logger::Debug("Batch invalidated " + std::to_string(chunksToInvalidate.size()) + " chunk meshes");
}

void World::RegenerateDirtyChunks() {
    // Regenerate meshes for all dirty chunks in one pass
    int regeneratedCount = 0;
    for (const auto& pair : loadedChunks) {
        Chunk* chunk = pair.second.get();
        if (chunk && chunk->IsDirty() && chunk->IsGenerated()) {
            chunk->GenerateMesh();
            regeneratedCount++;
        }
    }
    
    if (regeneratedCount > 0) {
        Logger::Debug("Regenerated " + std::to_string(regeneratedCount) + " dirty chunk meshes");
    }
}

void World::UpdateChunkNeighbors(const ChunkCoord& coord) {
    Chunk* chunk = GetChunk(coord);
    if (!chunk) {
        return;
    }
    
    // Set neighbor pointers
    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + NEIGHBOR_OFFSETS[i];
        Chunk* neighborChunk = GetChunk(neighborCoord);
        chunk->SetNeighbor(i, neighborChunk);
        
        // Set reverse neighbor
        if (neighborChunk) {
            int reverseDir = (i % 2 == 0) ? i + 1 : i - 1; // Flip direction
            neighborChunk->SetNeighbor(reverseDir, chunk);
        }
    }
}

ChunkCoord World::WorldPosToChunkCoord(const WorldPos& pos) const {
    return pos.ToChunkCoord();
}

glm::ivec3 World::WorldPosToLocalChunkPos(const WorldPos& pos) const {
    constexpr int CHUNK_SIZE = 16;
    return glm::ivec3(
        ((pos.x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
        ((pos.y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
        ((pos.z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE
    );
}

std::vector<ChunkCoord> World::GetChunksInRadius(const ChunkCoord& center, int radius) const {
    std::vector<ChunkCoord> chunks;
    
    for (int x = center.x - radius; x <= center.x + radius; ++x) {
        for (int y = center.y - radius; y <= center.y + radius; ++y) {
            for (int z = center.z - radius; z <= center.z + radius; ++z) {
                ChunkCoord coord(x, y, z);
                
                // Check if within cubic radius (Manhattan distance)
                int dx = std::abs(x - center.x);
                int dy = std::abs(y - center.y);
                int dz = std::abs(z - center.z);
                int distance = std::max({dx, dy, dz});
                
                if (distance <= radius) {
                    chunks.push_back(coord);
                }
            }
        }
    }
    
    return chunks;
}