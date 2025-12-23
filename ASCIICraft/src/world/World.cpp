#include <ASCIICraft/world/World.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/RendererCPU.hpp>
#include <ASCIIgL/renderer/RendererGPU.hpp>

#include <ASCIICraft/player/Player.hpp>

// Static member definition - ordered to match face indices in Chunk::GenerateMesh()
const ChunkCoord World::FACE_NEIGHBOR_OFFSETS[6] = {
    ChunkCoord(0, 1, 0),   // Face 0: Top (Y+)
    ChunkCoord(0, -1, 0),  // Face 1: Bottom (Y-)
    ChunkCoord(0, 0, 1),   // Face 2: North (Z+)
    ChunkCoord(0, 0, -1),  // Face 3: South (Z-)
    ChunkCoord(1, 0, 0),   // Face 4: East (X+)
    ChunkCoord(-1, 0, 0)   // Face 5: West (X-)
};

World::World(unsigned int renderDistance, const WorldPos& spawnPoint, unsigned int maxWorldChunkRadius) 
    : renderDistance(renderDistance)
    , spawnPoint(spawnPoint)
    , player(nullptr)
    , maxWorldChunkRadius(maxWorldChunkRadius)
    , terrainGenerator(std::make_unique<TerrainGenerator>()) {
    Logger::Info("World created");
}

World::~World() {
    Logger::Info("World destroyed");
}

void World::Update() {
    if (!player) {
        return;
    }
    
    // Step 1: Load/unload chunks based on player position
    {
        PROFILE_SCOPE("Update.UpdateChunkLoading");
        UpdateChunkLoading();
    }

    // Step 2: Regenerate meshes for dirty chunks (batch processing)
    {
        PROFILE_SCOPE("Update.RegenerateDirtyChunks");
        RegenerateDirtyChunks();
    }
}

void World::Render() {
    if (!player) {
        Logger::Warning("No player set for world rendering");
        return;
    }

    std::vector<Chunk*> visibleChunks = GetVisibleChunks(player->GetPosition(), player->GetCamera().getCamFront());
    Logger::Debug("Render: visibleChunks = " + std::to_string(visibleChunks.size()));

    Texture* batchTexture = nullptr;
    std::vector<std::vector<VERTEX>*> chunkVertexBatches;
    chunkVertexBatches.reserve(visibleChunks.size());

    for (Chunk* chunk : visibleChunks) {
        if (!chunk || !chunk->IsGenerated() || !chunk->HasMesh()) {
            // Logger::Debug("Render: Skipping invalid chunk");
            continue;
        }
        auto* mesh = chunk->GetMesh();
        if (!mesh || !mesh->texture || mesh->vertices.empty()) {
            Logger::Debug("Render: Skipping chunk with invalid mesh/texture/vertices");
            continue;
        }
        if (!batchTexture) batchTexture = mesh->texture;
        chunkVertexBatches.push_back(&mesh->vertices);
    }

    Logger::Debug("World::Render - Batches: " + std::to_string(chunkVertexBatches.size()) + 
                  ", Texture: " + (batchTexture ? "valid" : "NULL"));
    
    if (!chunkVertexBatches.empty() && batchTexture) {
        if (Renderer::GetInst().GetCpuOnly()) {
            RendererCPU::GetInst().GetVShader().SetMatrices(glm::mat4(1.0f), player->GetCamera().view, player->GetCamera().proj);
        } else {
            RendererGPU::GetInst().SetMVP(player->GetCamera().proj * player->GetCamera().view * glm::mat4(1.0f));
        }
        Renderer::GetInst().RenderTriangles(chunkVertexBatches, batchTexture);
    } else {
        Logger::Debug("Render: Skipping RenderTriangles, no valid batches or texture.");
    }
}

void World::GenerateWorld() {
    // Generate chunks in the surrounding area (cubic) within world limits
    // Cast to signed int to avoid unsigned wraparound issues
    int signedRenderDistance = static_cast<int>(renderDistance);
    
    std::vector<ChunkCoord> generatedChunks;
    
    for (int x = -signedRenderDistance; x <= signedRenderDistance; ++x) {
        for (int y = -signedRenderDistance; y <= signedRenderDistance; ++y) {  // Now includes Y-axis
            for (int z = -signedRenderDistance; z <= signedRenderDistance; ++z) {
                ChunkCoord chunkCoord(x, y, z);  // Full 3D chunk generation

                if (!IsChunkOutsideWorld(chunkCoord)) {
                    GetOrCreateChunk(chunkCoord);
                    generatedChunks.push_back(chunkCoord);
                }
            }
        }
    }
    
    Logger::Debug("Updating neighbors for " + std::to_string(generatedChunks.size()) + " chunks");
    for (const ChunkCoord& coord : generatedChunks) {
        UpdateChunkNeighbors(coord, true);
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
    if (coord.y < 0) {
        return; // Below world bounds
    }

    if (IsChunkLoaded(coord)) {
        return; // Already loaded
    }
    
    // Create and store new chunk
    auto chunk = std::make_unique<Chunk>(coord);
    Chunk* chunkPtr = chunk.get();
    loadedChunks[coord] = std::move(chunk);
    
    // Generate terrain using TerrainGenerator with callbacks for World operations
    auto setBlockQuiet = [this](int x, int y, int z, const Block& block, std::unordered_set<Chunk*>& affectedChunks) {
        this->SetBlockQuiet(x, y, z, block, affectedChunks);
    };
    
    terrainGenerator->GenerateChunk(chunkPtr, setBlockQuiet);
    
    Logger::Debug("Loaded chunk at (" + std::to_string(coord.x) + ", " + 
                  std::to_string(coord.y) + ", " + std::to_string(coord.z) + ")");
}

void World::UnloadChunk(const ChunkCoord& coord) {
    auto it = loadedChunks.find(coord);
    if (it == loadedChunks.end()) {
        return; // Already unloaded
    }
    
    Chunk* chunkToUnload = it->second.get();
    if (!chunkToUnload) {
        loadedChunks.erase(it);
        return;
    }
    
    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + FACE_NEIGHBOR_OFFSETS[i];
        
        // Use find() instead of GetChunk() to avoid issues during iteration
        auto neighborIt = loadedChunks.find(neighborCoord);
        if (neighborIt != loadedChunks.end() && neighborIt->second) {
            Chunk* neighborChunk = neighborIt->second.get();
            
            // Calculate reverse direction and clear the pointer
            int reverseDir = (i % 2 == 0) ? i + 1 : i - 1;
            neighborChunk->SetNeighbor(reverseDir, nullptr);
            
            // Mark neighbor as dirty since it lost a neighbor
            neighborChunk->SetDirty(true);
        }
    }
    
    // Now safe to unload the chunk
    loadedChunks.erase(it);
    Logger::Debug("Unloaded chunk at (" + std::to_string(coord.x) + ", " + 
                  std::to_string(coord.y) + ", " + std::to_string(coord.z) + ")");
}

bool World::IsChunkLoaded(const ChunkCoord& coord) const {
    return loadedChunks.find(coord) != loadedChunks.end();
}

void World::UpdateChunkLoading() {
    if (!player) {
        return;
    }
    
    glm::vec3 playerPos = player->GetPosition();
    ChunkCoord playerChunk = WorldPosToChunkCoord(WorldPos(playerPos));
    
    const unsigned int loadRadius = renderDistance;
    const unsigned int unloadRadius = renderDistance + 2;
    
    // === STEP 1: LOAD NEW CHUNKS ===
    std::vector<ChunkCoord> chunksToLoad = GetChunksInRadius(playerChunk, loadRadius);
    
    // Filter out chunks outside world limits
    chunksToLoad.erase(std::remove_if(chunksToLoad.begin(), chunksToLoad.end(),
        [this](const ChunkCoord& coord) {
            return IsChunkOutsideWorld(coord) || IsChunkLoaded(coord);
        }), chunksToLoad.end());

    Logger::Debug(std::to_string(loadedChunks.size()) + " chunks currently loaded");

    // Load missing chunks
    std::vector<ChunkCoord> newlyLoadedChunks;
        for (const ChunkCoord& coord : chunksToLoad) {
            LoadChunk(coord);
            newlyLoadedChunks.push_back(coord);
    }
    
    for (const ChunkCoord& coord : newlyLoadedChunks) {
        UpdateChunkNeighbors(coord, true);
    }

    // === STEP 2: UNLOAD DISTANT CHUNKS ===
    std::vector<ChunkCoord> chunksToUnload;
    
    for (const auto& pair : loadedChunks) {
        const ChunkCoord& coord = pair.first;
        int dx = coord.x - playerChunk.x;
        int dy = coord.y - playerChunk.y;
        int dz = coord.z - playerChunk.z;
        int distance = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
        
        if (distance > static_cast<int>(unloadRadius) || IsChunkOutsideWorld(coord)) {
            chunksToUnload.push_back(coord);
        }
    }
    
    // Unload chunks (simple and safe)
    for (const ChunkCoord& coord : chunksToUnload) {
        UnloadChunk(coord);
    }
    
    if (chunksToUnload.size() > 0) {
        Logger::Debug("Unloaded " + std::to_string(chunksToUnload.size()) + " distant chunks");
    }
}

std::vector<Chunk*> World::GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const {
    std::vector<Chunk*> visibleChunks;
    visibleChunks.reserve(loadedChunks.size()); // Pre-allocate to avoid reallocations
    
    // Get player's chunk coordinate for distance calculations
    ChunkCoord playerChunk = WorldPosToChunkCoord(WorldPos(playerPos));
    
    // Normalize view direction
    glm::vec3 forward = glm::normalize(viewDir);
    
    // FOV-based frustum culling with safety margin
    const Camera3D& camera = player->GetCamera();
    float fov = camera.GetFov();
    float extendedFov = fov * 1.5f; // 30° margin for safety
    const float fovHalfAngle = glm::radians(extendedFov * 0.5f);
    const float fovCosine = cos(fovHalfAngle);
    
    // Chunk bounding sphere radius (distance from center to corner)
    const float chunkRadius = Chunk::SIZE * 0.866025f; // sqrt(3)/2 * SIZE
    
    for (const auto& pair : loadedChunks) {
        const ChunkCoord& coord = pair.first;
        Chunk* chunk = pair.second.get();
        
        // Calculate Chebyshev distance (max of abs differences) for cubic chunks
        int dx = coord.x - playerChunk.x;
        int dy = coord.y - playerChunk.y;
        int dz = coord.z - playerChunk.z;
        int chunkDistance = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
        
        // Early rejection: chunks beyond render distance
        if (chunkDistance > renderDistance) {
            continue;
        }
        
        // Always include immediate area (player's chunk + 1 layer around)
        // This avoids frustum culling artifacts in the immediate vicinity
        if (chunkDistance <= 1) {
            visibleChunks.push_back(chunk);
            continue;
        }
        
        // Calculate chunk center in world coordinates
        glm::vec3 chunkCenter = glm::vec3(
            coord.x * Chunk::SIZE + Chunk::SIZE * 0.5f,
            coord.y * Chunk::SIZE + Chunk::SIZE * 0.5f,
            coord.z * Chunk::SIZE + Chunk::SIZE * 0.5f
        );
        
        // Vector from player to chunk center
        glm::vec3 toChunk = chunkCenter - playerPos;
        float euclideanDistance = glm::length(toChunk);
        
        // Skip if somehow at same position (avoid division by zero)
        if (euclideanDistance < 0.1f) {
            visibleChunks.push_back(chunk);
            continue;
        }
        
        // Normalize direction to chunk
        glm::vec3 toChunkNorm = toChunk / euclideanDistance;
        
        // === OPTIMIZED CONE-BASED FRUSTUM CULLING ===
        // Calculate dot product between view direction and direction to chunk
        float dotProduct = glm::dot(forward, toChunkNorm);
        
        // Fast approximation: instead of calculating angular size with atan,
        // use the ratio directly. For small angles: tan(θ) ≈ sin(θ) ≈ θ
        // This gives us: angularSize ≈ chunkRadius / euclideanDistance
        // 
        // The threshold needs to account for the chunk's angular extent:
        // adjustedThreshold = cos(fovHalfAngle + angularSize)
        // 
        // Using the approximation cos(a + b) ≈ cos(a) - b*sin(a) for small b:
        float angularExtent = chunkRadius / euclideanDistance;
        float adjustedThreshold = fovCosine - angularExtent * sin(fovHalfAngle);
        
        // Clamp threshold to reasonable range (prevent over-culling at extreme distances)
        adjustedThreshold = std::max(adjustedThreshold, -1.0f);
        
        // Include chunks that are within the extended FOV cone
        if (dotProduct >= adjustedThreshold) {
            visibleChunks.push_back(chunk);
        }
    }
    
    return visibleChunks;
}

void World::BatchInvalidateChunkFaceNeighborMeshes(const ChunkCoord& coord) {
    // Collect all chunks that need invalidation without triggering regeneration
    std::unordered_set<ChunkCoord> chunksToInvalidate;
    
    // Add the main chunk
    chunksToInvalidate.insert(coord);
    
    // Add neighboring chunks
    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + FACE_NEIGHBOR_OFFSETS[i];
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
    int regeneratedCount = 0;
    
    // Simple: regenerate all dirty chunks that are loaded
    for (const auto& pair : loadedChunks) {
        if (regeneratedCount >= MAX_REGENERATIONS_PER_FRAME) {
            Logger::Warning("Hit max regenerations per frame (" + std::to_string(MAX_REGENERATIONS_PER_FRAME) + "), deferring rest to next frame");
            break;
        }
        
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

void World::UpdateChunkNeighbors(const ChunkCoord& coord, bool markNeighborsDirty) {
    Chunk* chunk = GetChunk(coord);
    if (!chunk) {
        return;
    }
    
    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + FACE_NEIGHBOR_OFFSETS[i];
        Chunk* neighborChunk = GetChunk(neighborCoord);
        
        // Get old neighbor to check if it actually changed
        Chunk* oldNeighbor = chunk->GetNeighbor(i);
        chunk->SetNeighbor(i, neighborChunk);

        // Set reverse neighbor
        if (neighborChunk && neighborChunk != oldNeighbor && neighborChunk->IsGenerated() && chunk->IsGenerated()) {
            int reverseDir = (i % 2 == 0) ? i + 1 : i - 1;
            neighborChunk->SetNeighbor(reverseDir, chunk);
            
            // Only mark dirty if requested AND neighbor already has a mesh
            // During batch loading, we skip marking neighbors dirty to avoid cascade
            if (markNeighborsDirty && neighborChunk->HasMesh()) {
                neighborChunk->SetDirty(true);
            }
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

std::vector<ChunkCoord> World::GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const {
    std::vector<ChunkCoord> chunks;
    
    // CRITICAL: Cast radius to signed int to prevent integer overflow with negative coordinates
    int signedRadius = static_cast<int>(radius);
    
    for (int x = center.x - signedRadius; x <= center.x + signedRadius; ++x) {
        for (int y = center.y - signedRadius; y <= center.y + signedRadius; ++y) {
            for (int z = center.z - signedRadius; z <= center.z + signedRadius; ++z) {
                ChunkCoord coord(x, y, z);
                
                // Check if within cubic radius (Chebyshev distance)
                int dx = std::abs(x - center.x);
                int dy = std::abs(y - center.y);
                int dz = std::abs(z - center.z);
                int distance = std::max({dx, dy, dz});
                
                if (distance <= signedRadius) {
                    chunks.push_back(coord);
                }
            }
        }
    }
    
    return chunks;
}

bool World::IsChunkOutsideWorld(const ChunkCoord& coord) const {
    // Calculate distance from world origin (0, 0, 0)
    int dx = std::abs(coord.x);
    int dy = std::abs(coord.y);
    int dz = std::abs(coord.z);
    int distanceFromOrigin = std::max({dx, dy, dz});
    return distanceFromOrigin > maxWorldChunkRadius;
}

void World::SetBlockQuiet(int x, int y, int z, const Block& block, std::unordered_set<Chunk*>& affectedChunks) {
    ChunkCoord chunkCoord = WorldPosToChunkCoord(WorldPos(x, y, z));
    Chunk* chunk = GetChunk(chunkCoord);
    if (!chunk) return; // Skip if chunk not loaded (tree at edge)
    
    glm::ivec3 localPos = WorldPosToLocalChunkPos(WorldPos(x, y, z));
    chunk->SetBlock(localPos.x, localPos.y, localPos.z, block);
    affectedChunks.insert(chunk);
}

