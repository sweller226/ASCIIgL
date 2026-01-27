#include <ASCIICraft/world/ChunkManager.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>

#ifdef max
#  undef max
#endif

#include <algorithm>
#include <cmath>

#include <ASCIICraft/ecs/managers/PlayerManager.hpp>

// Static member definition - ordered to match face indices in Chunk::GenerateMesh()
const ChunkCoord ChunkManager::FACE_NEIGHBOR_OFFSETS[6] = {
    ChunkCoord(0, 1, 0),   // Face 0: Top (Y+)
    ChunkCoord(0, -1, 0),  // Face 1: Bottom (Y-)
    ChunkCoord(0, 0, 1),   // Face 2: North (Z+)
    ChunkCoord(0, 0, -1),  // Face 3: South (Z-)
    ChunkCoord(1, 0, 0),   // Face 4: East (X+)
    ChunkCoord(-1, 0, 0)   // Face 5: West (X-)
};

ChunkManager::ChunkManager(entt::registry& registry, const unsigned int chunkWorldLimit, const unsigned int renderDistance) 
    : maxWorldChunkRadius(chunkWorldLimit)
    , renderDistance(renderDistance)
    , registry(registry) 
    , terrainGenerator() {
        ASCIIgL::Logger::Debug("Chunk manager initialized");
        regionManager = std::make_unique<RegionManager>();
}

Chunk* ChunkManager::GetChunk(const ChunkCoord& coord) {
    auto it = loadedChunks.find(coord);
    if (it != loadedChunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk* ChunkManager::GetOrCreateChunk(const ChunkCoord& coord) {
    Chunk* chunk = GetChunk(coord);
    if (!chunk) {
        LoadChunk(coord);
        chunk = GetChunk(coord);
    }
    return chunk;
}

void ChunkManager::LoadChunk(const ChunkCoord& coord) {

    if (coord.y < 0) {
        return;
    }

    if (IsChunkLoaded(coord)) {
        return;
    }

    // Create and store new chunk
    auto chunk = std::make_unique<Chunk>(coord);
    Chunk* chunkPtr = chunk.get();
    loadedChunks[coord] = std::move(chunk);

    // Resolve region
    RegionCoord rp = coord.ToRegionCoord();

    if (!regionManager->FilePresent(rp)) {
        regionManager->AddRegion(RegionFile(rp));
    }
    
    // Access region once
    RegionFile& region = regionManager->AccessRegion(rp);

    // Try loading chunk from region file with error handling for corruption
    bool loadedFromFile = false;
    try {
        loadedFromFile = region.LoadChunk(chunkPtr);
    } catch (const std::exception& e) {
        ASCIIgL::Logger::Warningf("Failed to load chunk (%d,%d,%d) from region file: %s. Regenerating.",
                                   coord.x, coord.y, coord.z, e.what());
        loadedFromFile = false;
    } catch (...) {
        ASCIIgL::Logger::Warningf("Failed to load chunk (%d,%d,%d) from region file: unknown error. Regenerating.",
                                   coord.x, coord.y, coord.z);
        loadedFromFile = false;
    }

    if (loadedFromFile) {
        // Loaded from file - mark as generated so mesh can be built
        chunkPtr->SetGenerated(true);
    } else {

        auto setBlock = [this](int x, int y, int z, const Block& block) {
            this->SetBlock(x, y, z, block);
        };

        terrainGenerator.GenerateChunk(chunkPtr, setBlock);
    }

    // Load metadata (cross‑chunk edits stored in region file)
    auto cachedMetaBucket = std::make_unique<MetaBucket>();
    region.LoadMetaData(coord, cachedMetaBucket.get());

    // Apply cached edits
    for (const auto& edit : cachedMetaBucket->edits) {
        int x = 0, y = 0, z = 0;
        edit.UnpackPos(x, y, z);
        chunkPtr->SetBlock(x, y, z, edit.block);
    }
    cachedMetaBucket->edits.clear();

    // Apply in‑memory pending edits
    auto it = crossChunkEdits.find(coord);
    if (it != crossChunkEdits.end()) {
        MetaBucket& bucket = it->second;

        for (const auto& edit : bucket.edits) {
            int x = 0, y = 0, z = 0;
            edit.UnpackPos(x, y, z);
            chunkPtr->SetBlock(x, y, z, edit.block);
        }

        crossChunkEdits.erase(it);
    } else {}
}

void ChunkManager::UnloadChunk(const ChunkCoord& coord) {
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
    
    // Now safe to unload the 
    RegionCoord rp = coord.ToRegionCoord();
    if (!regionManager->FilePresent(rp)) {
        regionManager->AddRegion(RegionFile(rp));
    }
    regionManager->AccessRegion(rp).SaveChunk(it->second.get());

    loadedChunks.erase(it);
    ASCIIgL::Logger::Debug("Unloaded chunk at (" + std::to_string(coord.x) + ", " + 
                  std::to_string(coord.y) + ", " + std::to_string(coord.z) + ")");
}

bool ChunkManager::IsChunkLoaded(const ChunkCoord& coord) const {
    return loadedChunks.find(coord) != loadedChunks.end();
}

void ChunkManager::UpdateChunkLoading() {
    if (!ecs::managers::GetPlayerPtr(registry)) {
        return;
    }
    
    glm::vec3 playerPos = ecs::managers::GetPlayerPtr(registry)->GetPosition();
    ChunkCoord playerChunk = WorldCoord(playerPos).ToChunkCoord();
    
    const unsigned int loadRadius = renderDistance;
    const unsigned int unloadRadius = renderDistance + 2;

    // === STEP 0: CACHE OLD METADATA ===
    const uint32_t now = NowSeconds();
    constexpr int MAX_META_SAVES_PER_FRAME = 4;
    int metaSaveCount = 0;

    // Process queue entries, but check actual timestamps since queue may not be strictly ordered
    size_t queueSize = metaTimeTracker.size();
    for (size_t i = 0; i < queueSize && metaSaveCount < MAX_META_SAVES_PER_FRAME; ++i) {
        const ChunkCoord key = metaTimeTracker.front();
        metaTimeTracker.pop();

        auto it = crossChunkEdits.find(key);
        if (it == crossChunkEdits.end()) {
            // Bucket was already removed earlier
            continue;
        }

        MetaBucket& bucket = it->second;

        // Check actual timestamp, not queue order
        if (now - bucket.lastTouched < META_BUCKET_TIME_LIMIT) {
            // Still too new, re-add to back of queue for later processing
            metaTimeTracker.push(key);
            continue;
        }

        // Otherwise, it's expired — cache it in region file
        RegionCoord rp = key.ToRegionCoord();
        if (!regionManager->FilePresent(rp)) {
            regionManager->AddRegion(RegionFile(rp));
        }

        regionManager->AccessRegion(rp).SaveMetaData(key, &it->second);
        crossChunkEdits.erase(it);
        metaSaveCount++;
    }

    // === STEP 1: LOAD NEW CHUNKS ===
    std::vector<ChunkCoord> chunksToLoad = GetChunksInRadius(playerChunk, loadRadius);
    
    // Filter out chunks outside world limits
    chunksToLoad.erase(std::remove_if(chunksToLoad.begin(), chunksToLoad.end(),
        [this](const ChunkCoord& coord) {
            return IsChunkOutsideWorld(coord) || IsChunkLoaded(coord);
        }), chunksToLoad.end());

    ASCIIgL::Logger::Debug(std::to_string(loadedChunks.size()) + " chunks currently loaded");

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
        ASCIIgL::Logger::Debug("Unloaded " + std::to_string(chunksToUnload.size()) + " distant chunks");
    }
}

std::vector<Chunk*> ChunkManager::GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const {
    std::vector<Chunk*> visibleChunks;
    visibleChunks.reserve(loadedChunks.size()); // Pre-allocate to avoid reallocations
    
    // Get player's chunk coordinate for distance calculations
    ChunkCoord playerChunk = WorldCoord(playerPos).ToChunkCoord();
    
    // Normalize view direction
    glm::vec3 forward = glm::normalize(viewDir);
    
    // FOV-based frustum culling with safety margin
    const ASCIIgL::Camera3D* camera = ecs::managers::GetPlayerPtr(registry)->GetCamera();
    float fov = camera->GetFov();
    float extendedFov = fov * 1.6f; // 30° margin for safety
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

void ChunkManager::BatchInvalidateChunkFaceNeighborMeshes(const ChunkCoord& coord) {
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
    
    ASCIIgL::Logger::Debug("Batch invalidated " + std::to_string(chunksToInvalidate.size()) + " chunk meshes");
}

void ChunkManager::RegenerateDirtyChunks() {
    int regeneratedCount = 0;
    
    // Simple: regenerate all dirty chunks that are loaded
    for (const auto& pair : loadedChunks) {
        if (regeneratedCount >= MAX_REGENERATIONS_PER_FRAME) {
            ASCIIgL::Logger::Warning("Hit max regenerations per frame (" + std::to_string(MAX_REGENERATIONS_PER_FRAME) + "), deferring rest to next frame");
            break;
        }
        
        Chunk* chunk = pair.second.get();
        
        if (chunk && chunk->IsDirty() && chunk->IsGenerated()) {
            chunk->GenerateMesh();
            regeneratedCount++;
        }
    }
    
    if (regeneratedCount > 0) {
        ASCIIgL::Logger::Debug("Regenerated " + std::to_string(regeneratedCount) + " dirty chunk meshes");
    }
}

void ChunkManager::UpdateChunkNeighbors(const ChunkCoord& coord, bool markNeighborsDirty) {
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

std::vector<ChunkCoord> ChunkManager::GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const {
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

bool ChunkManager::IsChunkOutsideWorld(const ChunkCoord& coord) const {
    // Calculate distance from world origin (0, 0, 0)
    int dx = std::abs(coord.x);
    int dy = std::abs(coord.y);
    int dz = std::abs(coord.z);
    int distanceFromOrigin = std::max({dx, dy, dz});
    return distanceFromOrigin > maxWorldChunkRadius;
}

Block& ChunkManager::GetBlock(const WorldCoord& pos) {
    return GetBlock(pos.x, pos.y, pos.z);
}

Block& ChunkManager::GetBlock(int x, int y, int z) {
    ChunkCoord chunkCoord = WorldCoord(x, y, z).ToChunkCoord();
    
    Chunk* chunk = GetChunk(chunkCoord);
    if (!chunk) {
        return Block(); // Return air block if chunk not loaded
    }
    
    glm::ivec3 localPos = WorldCoord(x, y, z).ToLocalChunkPos();
    return chunk->GetBlock(localPos.x, localPos.y, localPos.z);
}

void ChunkManager::SetBlock(const WorldCoord& pos, const Block& block) {
    SetBlock(pos.x, pos.y, pos.z, block);
}

void ChunkManager::SetBlock(int x, int y, int z, const Block& block) {
    ChunkCoord chunkCoord = WorldCoord(x, y, z).ToChunkCoord();
    
    Chunk* chunk = GetChunk(chunkCoord);
    if (!chunk) {
        CrossChunkEdit crossChunkEdit;
        crossChunkEdit.PackPos(x, y, z);
        crossChunkEdit.block = block;

        auto it = crossChunkEdits.find(chunkCoord);
        if (it == crossChunkEdits.end()) {
            MetaBucket metaBucket;
            metaBucket.edits.push_back(crossChunkEdit);
            crossChunkEdits.insert({chunkCoord, metaBucket});
            metaTimeTracker.push(chunkCoord);
        } else {
            it->second.edits.push_back(crossChunkEdit);
            it->second.lastTouched = NowSeconds(); // getting curr time in seconds
        }
    } else {
        glm::ivec3 localPos = WorldCoord(x, y, z).ToLocalChunkPos();
        chunk->SetBlock(localPos.x, localPos.y, localPos.z, block);
        chunk->SetDirty(true);

        BlockUpdateNeighboursDirty(chunkCoord, localPos);
    }
}

void ChunkManager::Update() {
    UpdateChunkLoading();
    RegenerateDirtyChunks();
}

void ChunkManager::RenderChunks() {
    ASCIIgL::Logger::Debug("ChunkManager::RenderChunks called.");

    // --- Player retrieval ---
    const auto player = ecs::managers::GetPlayerPtr(registry);
    if (!player) {
        ASCIIgL::Logger::Error("RenderChunks: PlayerManager returned NULL player!");
        return;
    }

    glm::vec3 pos = player->GetPosition();
    ASCIIgL::Logger::Debug("RenderChunks: Player position = (" +
        std::to_string(pos.x) + ", " +
        std::to_string(pos.y) + ", " +
        std::to_string(pos.z) + ")");

    // --- Camera retrieval ---
    auto* cam = player->GetCamera();
    if (!cam) {
        ASCIIgL::Logger::Error("RenderChunks: PlayerCamera is NULL!");
        return;
    }

    glm::vec3 camFront = cam->getCamFront();
    ASCIIgL::Logger::Debug("RenderChunks: Camera front = (" +
        std::to_string(camFront.x) + ", " +
        std::to_string(camFront.y) + ", " +
        std::to_string(camFront.z) + ")");

    // --- Visible chunks ---
    std::vector<Chunk*> visibleChunks = GetVisibleChunks(pos, camFront);
    ASCIIgL::Logger::Debug("RenderChunks: visibleChunks.size() = " +
        std::to_string(visibleChunks.size()));

    // --- Material retrieval ---
    auto mat = ASCIIgL::MaterialLibrary::GetInst().GetDefault();
    if (!mat) {
        ASCIIgL::Logger::Error("RenderChunks: Default material is NULL!");
        return;
    }

    // --- MVP setup ---
    glm::mat4 mvp = cam->proj * cam->view * glm::mat4(1.0f);
    ASCIIgL::Logger::Debug("RenderChunks: MVP matrix computed.");

    ASCIIgL::RendererGPU::GetInst().BindMaterial(mat.get());
    mat->SetMatrix4("mvp", mvp);
    ASCIIgL::RendererGPU::GetInst().UploadMaterialConstants(mat.get());

    // --- Render chunks ---
    int renderedCount = 0;

    for (Chunk* chunk : visibleChunks) {
        if (!chunk) {
            ASCIIgL::Logger::Debug("RenderChunks: Skipping NULL chunk pointer.");
            continue;
        }

        if (!chunk->IsGenerated()) {
            ASCIIgL::Logger::Debug("RenderChunks: Skipping ungenerated chunk.");
            continue;
        }

        chunk->Render();
        renderedCount++;
    }

    ASCIIgL::Logger::Debug("RenderChunks: Rendered " + std::to_string(renderedCount) + " chunks.");
}

std::pair<Block*, WorldCoord>  ChunkManager::BlockIntersectsView(glm::vec3& lookDir, glm::vec3& headPos, float reach) {
    glm::vec3 dir = glm::normalize(lookDir);
    const float step = 0.1f;

    float dist = 0.0f;
    while (dist <= reach) {
        glm::vec3 pos = headPos + dir * dist;

        int bx = static_cast<int>(floor(pos.x));
        int by = static_cast<int>(floor(pos.y));
        int bz = static_cast<int>(floor(pos.z));

        Block& block = GetBlock(bx, by, bz);

        if (block.type != BlockType::Air) {
            // Return pointer + world coordinate of the hit block
            return { &block, WorldCoord(bx, by, bz) };
        }

        dist += step;
    }

    return { nullptr, WorldCoord() };
}

std::pair<bool, WorldCoord> ChunkManager::BlockIntersectsViewForPlacement(glm::vec3& lookDir, glm::vec3& headPos, float reach) {
    glm::vec3 dir = glm::normalize(lookDir);
    const float step = 0.1f;

    float dist = 0.0f;

    // Track the last empty block position
    WorldCoord lastEmpty;

    while (dist <= reach) {
        glm::vec3 pos = headPos + dir * dist;

        int bx = static_cast<int>(floor(pos.x));
        int by = static_cast<int>(floor(pos.y));
        int bz = static_cast<int>(floor(pos.z));

        Block& block = GetBlock(bx, by, bz);

        if (block.type == BlockType::Air) {
            // Update last empty block
            lastEmpty = WorldCoord(bx, by, bz);
        }
        else {
            // Hit a solid block — return the *previous* empty block
            Block& emptyBlock = GetBlock(lastEmpty.x, lastEmpty.y, lastEmpty.z);
            return { true, lastEmpty };
        }

        dist += step;
    }

    return { false, WorldCoord() };
}

void ChunkManager::BlockUpdateNeighboursDirty(const ChunkCoord& chunkCoord, const glm::ivec3& localPos) {
    // Lambda to safely mark a chunk dirty
    auto mark = [&](const ChunkCoord& cc) {
        if (Chunk* c = GetChunk(cc)) {
            c->SetDirty(true);
        }
    };

    // X-axis neighbors
    if (localPos.x == 0)
        mark(chunkCoord + ChunkCoord(-1, 0, 0));
    if (localPos.x == CHUNK_SIZE - 1)
        mark(chunkCoord + ChunkCoord(1, 0, 0));

    // Y-axis neighbors
    if (localPos.y == 0)
        mark(chunkCoord + ChunkCoord(0, -1, 0));
    if (localPos.y == CHUNK_SIZE - 1)
        mark(chunkCoord + ChunkCoord(0, 1, 0));

    // Z-axis neighbors
    if (localPos.z == 0)
        mark(chunkCoord + ChunkCoord(0, 0, -1));
    if (localPos.z == CHUNK_SIZE - 1)
        mark(chunkCoord + ChunkCoord(0, 0, 1));
}
