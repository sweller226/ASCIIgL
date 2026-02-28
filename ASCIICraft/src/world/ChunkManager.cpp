#include <ASCIICraft/world/ChunkManager.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/ChunkMeshGen.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>

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
    , terrainGenerator(registry) {
    ASCIIgL::Logger::Debug("Chunk manager initialized");
    regionManager = std::make_unique<RegionManager>();
    chunkJobQueue = std::make_unique<ChunkJobQueue>(registry);
    chunkJobQueue->SetTerrainGenerator(&terrainGenerator);
    chunkJobQueue->SetMaxDrainPerFrame(static_cast<size_t>(MAX_QUEUES_PER_FRAME));
    chunkJobQueue->SetMaxDrainMeshPerFrame(static_cast<size_t>(MAX_MESH_APPLIES_PER_FRAME));
    UpdateFogFromRenderDistance();
    glm::ivec3 bgCol = ASCIIgL::Renderer::GetInst().GetBackgroundCol();
    fogParams_.fogColor = glm::vec3(bgCol) / 255.0f;
}

void ChunkManager::SetRenderDistance(unsigned int distance) {
    renderDistance = distance;
    UpdateFogFromRenderDistance();
}

void ChunkManager::UpdateFogFromRenderDistance() {
    float chunkRenderDist = static_cast<float>(renderDistance * Chunk::SIZE);
    fogParams_.fogEnd = chunkRenderDist - (Chunk::SIZE * 1.5f);
    fogParams_.fogStart = fogParams_.fogEnd - (Chunk::SIZE * 1.0f);
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

    // Load metadata (cross‑chunk edits stored in region file)
    auto cachedMetaBucket = std::make_unique<MetaBucket>();
    region.LoadMetaData(coord, cachedMetaBucket.get());

    if (loadedFromFile) {
        chunkPtr->SetGenerated(true);
        // Apply cached edits and in-memory edits immediately
        for (const auto& edit : cachedMetaBucket->edits) {
            int x = 0, y = 0, z = 0;
            edit.UnpackPos(x, y, z);
            chunkPtr->SetBlockState(x, y, z, edit.stateId);
        }
        auto it = crossChunkEdits.find(coord);
        if (it != crossChunkEdits.end()) {
            for (const auto& edit : it->second.edits) {
                int x = 0, y = 0, z = 0;
                edit.UnpackPos(x, y, z);
                chunkPtr->SetBlockState(x, y, z, edit.stateId);
            }
            crossChunkEdits.erase(it);
        }
    } else {
        // Enqueue terrain gen; keep metadata/edits in crossChunkEdits to apply when terrain result is drained
        std::vector<CrossChunkEdit> inMemoryEdits;
        auto it = crossChunkEdits.find(coord);
        if (it != crossChunkEdits.end()) {
            inMemoryEdits = std::move(it->second.edits);
            crossChunkEdits.erase(it);
        }
        crossChunkEdits[coord].edits = std::move(cachedMetaBucket->edits);
        for (auto& edit : inMemoryEdits)
            crossChunkEdits[coord].edits.push_back(std::move(edit));
        chunkJobQueue->EnqueueTerrainGen(chunkPtr);
    }
}

void ChunkManager::SaveAll() {
    if (chunkJobQueue) {
        chunkJobQueue->WaitForPending();
        // Apply any completed terrain/mesh results so we save up-to-date chunk data
        DrainAndApplyJobResults();
    }
    ASCIIgL::Logger::Info("Saving all chunks (" + std::to_string(loadedChunks.size()) + ") and metadata...");

    // 1. Save all loaded chunks
    for (auto& [coord, chunkPtr] : loadedChunks) {
        if (!chunkPtr) continue;
        
        RegionCoord rp = coord.ToRegionCoord();
        if (!regionManager->FilePresent(rp)) {
            regionManager->AddRegion(RegionFile(rp));
        }
        regionManager->AccessRegion(rp).SaveChunk(chunkPtr.get());
    }
    loadedChunks.clear();

    // 2. Save all pending metadata (cross-chunk edits)
    int metaCount = 0;
    for (auto& [coord, metaBucket] : crossChunkEdits) {
        if (metaBucket.edits.empty()) continue;

        RegionCoord rp = coord.ToRegionCoord();
        if (!regionManager->FilePresent(rp)) {
            regionManager->AddRegion(RegionFile(rp));
        }
        regionManager->AccessRegion(rp).SaveMetaData(coord, &metaBucket);
        metaCount++;
    }
    crossChunkEdits.clear();

    ASCIIgL::Logger::Info("SaveAll complete. Saved " + std::to_string(metaCount) + " meta buckets.");
}

void ChunkManager::UnloadChunk(const ChunkCoord& coord) {
    auto itChunk = loadedChunks.find(coord);
    if (itChunk == loadedChunks.end()) {
        return; // Already unloaded
    }

    Chunk* chunkToUnload = itChunk->second.get();
    if (!chunkToUnload) {
        loadedChunks.erase(itChunk);
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
    
    // Now safe to unload
    RegionCoord rp = coord.ToRegionCoord();
    if (!regionManager->FilePresent(rp)) {
        regionManager->AddRegion(RegionFile(rp));
    }
    regionManager->AccessRegion(rp).SaveChunk(itChunk->second.get());
    loadedChunks.erase(itChunk);

    auto itMeta = crossChunkEdits.find(coord);
    if (itMeta != crossChunkEdits.end()) {
        regionManager->AccessRegion(rp).SaveMetaData(coord, &itMeta->second);
        crossChunkEdits.erase(itMeta);
    }
}

bool ChunkManager::IsChunkLoaded(const ChunkCoord& coord) const {
    return loadedChunks.find(coord) != loadedChunks.end();
}

void ChunkManager::UpdateChunkLoading() {
    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player == entt::null) {
        return;
    }
    
    auto [playerPos, success] = ecs::components::GetPos(player, registry);
    if (!success) return;
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

    // Load closer chunks first (Chebyshev distance) so terrain/mesh generate from center outward
    std::sort(chunksToLoad.begin(), chunksToLoad.end(), [&playerChunk](const ChunkCoord& a, const ChunkCoord& b) {
        int da = std::max({std::abs(a.x - playerChunk.x), std::abs(a.y - playerChunk.y), std::abs(a.z - playerChunk.z)});
        int db = std::max({std::abs(b.x - playerChunk.x), std::abs(b.y - playerChunk.y), std::abs(b.z - playerChunk.z)});
        return da < db;
    });

    // Load missing chunks (cap per frame to spread disk I/O and avoid spikes)
    std::vector<ChunkCoord> newlyLoadedChunks;
    newlyLoadedChunks.reserve(std::min(chunksToLoad.size(), static_cast<size_t>(MAX_CHUNK_LOADS_PER_FRAME)));
    for (size_t i = 0; i < chunksToLoad.size() && i < static_cast<size_t>(MAX_CHUNK_LOADS_PER_FRAME); ++i) {
        const ChunkCoord& coord = chunksToLoad[i];
        LoadChunk(coord);
        newlyLoadedChunks.push_back(coord);
    }
    
    for (const ChunkCoord& coord : newlyLoadedChunks)
        UpdateChunkNeighbors(coord);

    // Enqueue mesh generation for newly loaded chunks only when all neighbors have terrain
    for (const ChunkCoord& coord : newlyLoadedChunks) {
        Chunk* c = GetChunk(coord);
        if (c && c->IsGenerated() && !c->HasMesh() && AllNeighborsGenerated(coord))
            chunkJobQueue->EnqueueMeshGen(c);
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
    for (const ChunkCoord& coord : chunksToUnload)
        UnloadChunk(coord);
}

std::vector<Chunk*> ChunkManager::GetVisibleChunks(const glm::vec3& playerPos, const glm::vec3& viewDir) const {
    std::vector<Chunk*> visibleChunks;
    visibleChunks.reserve(loadedChunks.size()); // Pre-allocate to avoid reallocations
    
    // Get player's chunk coordinate for distance calculations
    ChunkCoord playerChunk = WorldCoord(playerPos).ToChunkCoord();
    
    // Normalize view direction
    glm::vec3 forward = glm::normalize(viewDir);
    
    // FOV-based frustum culling with safety margin
    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player == entt::null) return visibleChunks;
    
    const ASCIIgL::Camera3D* camera = ecs::components::GetPlayerCamera(player, registry);
    if (!camera) return visibleChunks;
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

void ChunkManager::DrainAndApplyJobResults() {
    chunkJobQueue->DrainCompletedTerrainResultsInto(drainTerrainBuffer_);
    for (auto& r : drainTerrainBuffer_) {
        Chunk* c = GetChunk(r.coord);
        if (!c) continue;
        c->ApplyBlockData(r.result.blocks.data(), r.result.blocks.size());
        auto metaIt = crossChunkEdits.find(r.coord);
        if (metaIt != crossChunkEdits.end()) {
            for (const auto& edit : metaIt->second.edits) {
                int x = 0, y = 0, z = 0;
                edit.UnpackPos(x, y, z);
                c->SetBlockState(x, y, z, edit.stateId);
            }
            crossChunkEdits.erase(metaIt);
        }
        for (const auto& placement : r.result.crossChunkBlocks)
            SetBlockState(placement.pos.x, placement.pos.y, placement.pos.z, placement.stateId);
        if (c->IsGenerated()) {
            // Mark neighbors dirty so their meshes are recomputed with up-to-date adjacency,
            // which fixes cross-chunk border culling when new terrain arrives.
            UpdateChunkNeighbors(r.coord);
            if (AllNeighborsGenerated(r.coord))
                chunkJobQueue->EnqueueMeshGen(c);
        }
    }

    // Apply completed mesh results
    auto blockTextures = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
    if (!blockTextures || !blockTextures->IsValid()) return;
    ASCIIgL::TextureArray* texArray = blockTextures.get();

    chunkJobQueue->DrainCompletedMeshResultsInto(drainMeshBuffer_);
    for (auto& r : drainMeshBuffer_) {
        Chunk* c = GetChunk(r.coord);
        if (c) {
            c->ApplyMeshData(std::move(r.data), texArray);
            c->SetDirty(false);
        }
    }
}

void ChunkManager::RebuildChunkMeshImmediate(Chunk* c) {
    if (!c) return;
    auto* bsr = registry.ctx().find<blockstate::BlockStateRegistry>();
    if (!bsr) return;
    auto blockTextures = ASCIIgL::TextureLibrary::GetInst().GetTextureArray("terrainTextureArray");
    if (!blockTextures || !blockTextures->IsValid()) return;
    ASCIIgL::TextureArray* texArray = blockTextures.get();

    ChunkCoord coord = c->GetCoord();
    std::vector<uint32_t> chunkBlocks(Chunk::VOLUME);
    std::memcpy(chunkBlocks.data(), c->GetBlockData(), Chunk::VOLUME * sizeof(uint32_t));

    std::array<std::vector<uint32_t>, 6> neighborBlocks;
    std::array<const uint32_t*, 6> ptrs{};
    for (int i = 0; i < 6; ++i) {
        Chunk* neighbor = c->GetNeighbor(i);
        if (neighbor) {
            neighborBlocks[i].resize(Chunk::VOLUME);
            std::memcpy(neighborBlocks[i].data(), neighbor->GetBlockData(), Chunk::VOLUME * sizeof(uint32_t));
            ptrs[i] = neighborBlocks[i].data();
        } else {
            ptrs[i] = nullptr;
        }
    }

    ChunkMeshData data = BuildChunkMeshData(coord, chunkBlocks.data(), ptrs, bsr);
    c->ApplyMeshData(std::move(data), texArray);
}

void ChunkManager::EnqueueMeshForDirtyChunks() {
    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player == entt::null) return;
    auto [playerPos, success] = ecs::components::GetPos(player, registry);
    if (!success) return;
    ChunkCoord playerChunk = WorldCoord(playerPos).ToChunkCoord();

    std::vector<std::pair<ChunkCoord, Chunk*>> eligible;
    eligible.reserve(loadedChunks.size());
    for (const auto& pair : loadedChunks) {
        Chunk* chunk = pair.second.get();
        if (chunk && chunk->IsDirty() && chunk->IsGenerated() && AllNeighborsGenerated(pair.first))
            eligible.push_back({pair.first, chunk});
    }
    if (eligible.empty()) return;

    std::sort(eligible.begin(), eligible.end(), [&playerChunk](const auto& a, const auto& b) {
        int da = std::max({std::abs(a.first.x - playerChunk.x), std::abs(a.first.y - playerChunk.y), std::abs(a.first.z - playerChunk.z)});
        int db = std::max({std::abs(b.first.x - playerChunk.x), std::abs(b.first.y - playerChunk.y), std::abs(b.first.z - playerChunk.z)});
        return da < db;
    });

    int syncCount = 0;
    int enqueued = 0;
    for (const auto& [coord, chunk] : eligible) {
        if (syncCount < MAX_SYNC_MESH_REBUILDS_PER_FRAME) {
            RebuildChunkMeshImmediate(chunk);
            chunk->SetDirty(false);
            syncCount++;
        } else if (enqueued < MAX_QUEUES_PER_FRAME) {
            chunkJobQueue->EnqueueMeshGen(chunk);
            enqueued++;
        }
    }
}

bool ChunkManager::AllNeighborsGenerated(const ChunkCoord& coord) const {
    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + FACE_NEIGHBOR_OFFSETS[i];
        if (IsChunkOutsideWorld(neighborCoord))
            continue;
        auto it = loadedChunks.find(neighborCoord);
        if (it == loadedChunks.end() || !it->second || !it->second->IsGenerated())
            return false;
    }
    return true;
}

void ChunkManager::UpdateChunkNeighbors(const ChunkCoord& coord) {
    Chunk* chunk = GetChunk(coord);
    if (!chunk) return;

    for (int i = 0; i < 6; ++i) {
        ChunkCoord neighborCoord = coord + FACE_NEIGHBOR_OFFSETS[i];
        Chunk* neighborChunk = GetChunk(neighborCoord);
        chunk->SetNeighbor(i, neighborChunk);

        if (!neighborChunk || !neighborChunk->IsGenerated() || !chunk->IsGenerated())
            continue;

        int reverseDir = (i % 2 == 0) ? i + 1 : i - 1;
        neighborChunk->SetNeighbor(reverseDir, chunk);
        neighborChunk->SetDirty(true);
    }
}

std::vector<ChunkCoord> ChunkManager::GetChunksInRadius(const ChunkCoord& center, unsigned int radius) const {
    int signedRadius = static_cast<int>(radius);
    int side = 2 * signedRadius + 1;
    std::vector<ChunkCoord> chunks;
    chunks.reserve(static_cast<size_t>(side) * side * side);

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

uint32_t ChunkManager::GetBlockState(const WorldCoord& pos) const {
    return GetBlockState(pos.x, pos.y, pos.z);
}

uint32_t ChunkManager::GetBlockState(int x, int y, int z) const {
    ChunkCoord chunkCoord = WorldCoord(x, y, z).ToChunkCoord();
    
    auto it = loadedChunks.find(chunkCoord);
    if (it == loadedChunks.end() || !it->second) {
        return blockstate::BlockStateRegistry::AIR_STATE_ID;
    }
    
    glm::ivec3 localPos = WorldCoord(x, y, z).ToLocalChunkPos();
    return it->second->GetBlockState(localPos.x, localPos.y, localPos.z);
}

void ChunkManager::SetBlockState(const WorldCoord& pos, uint32_t stateId) {
    SetBlockState(pos.x, pos.y, pos.z, stateId);
}

void ChunkManager::SetBlockState(int x, int y, int z, uint32_t stateId) {
    ChunkCoord chunkCoord = WorldCoord(x, y, z).ToChunkCoord();
    
    Chunk* chunk = GetChunk(chunkCoord);
    glm::ivec3 localPos = WorldCoord(x, y, z).ToLocalChunkPos();

    // If the chunk doesn't exist yet OR its terrain hasn't been applied yet,
    // store the edit in crossChunkEdits so it can be applied after terrain.
    if (!chunk || !chunk->IsGenerated()) {
        CrossChunkEdit crossChunkEdit;
        crossChunkEdit.PackPos(localPos.x, localPos.y, localPos.z);
        crossChunkEdit.stateId = stateId;

        auto it = crossChunkEdits.find(chunkCoord);
        if (it == crossChunkEdits.end()) {
            MetaBucket metaBucket;
            metaBucket.edits.push_back(crossChunkEdit);
            crossChunkEdits.insert({chunkCoord, metaBucket});
            metaTimeTracker.push(chunkCoord);
        } else {
            it->second.edits.push_back(crossChunkEdit);
            it->second.lastTouched = NowSeconds();
        }
    } else {
        chunk->SetBlockState(localPos.x, localPos.y, localPos.z, stateId);
        chunk->SetDirty(true);

        BlockUpdateNeighboursDirty(chunkCoord, localPos);
    }
}

void ChunkManager::Update() {
    DrainAndApplyJobResults();
    UpdateChunkLoading();
    EnqueueMeshForDirtyChunks();
}

void ChunkManager::RenderChunks() {
    entt::entity player = ecs::components::GetPlayerEntity(registry);
    if (player == entt::null) {
        ASCIIgL::Logger::Error("RenderChunks: Player entity not found!");
        return;
    }

    auto [pos, success] = ecs::components::GetPos(player, registry);
    if (!success) return;

    auto* cam = ecs::components::GetPlayerCamera(player, registry);
    if (!cam) {
        ASCIIgL::Logger::Error("RenderChunks: PlayerCamera is NULL!");
        return;
    }

    glm::vec3 camFront = cam->getCamFront();
    std::vector<Chunk*> visibleChunks = GetVisibleChunks(pos, camFront);

    // --- Material retrieval ---
    auto mat = ASCIIgL::MaterialLibrary::GetInst().Get("blockMaterial");
    if (!mat) {
        ASCIIgL::Logger::Error("RenderChunks: blockMaterial not found!");
        return;
    }

    // --- MVP setup ---
    glm::mat4 mvp = cam->proj * cam->view * glm::mat4(1.0f);
    mat->SetMatrix4("mvp", mvp);

    // --- Fog (from ChunkManagerFogParams; start/end tied to render distance) ---
    mat->SetFloat3("cameraPos", pos);
    mat->SetFloat4("fogParams", glm::vec4(fogParams_.fogStart, fogParams_.fogEnd, 0.0f, 0.0f));
    mat->SetFloat3("fogColor", fogParams_.fogColor);

    // --- Prepare renderer draw-calls ---
    ASCIIgL::Renderer& renderer = ASCIIgL::Renderer::GetInst();

    int renderedCount = 0;

    // Precompute normalized camera forward for transparent depth sorting
    glm::vec3 camDir = glm::normalize(camFront);

    for (Chunk* chunk : visibleChunks) {
        if (!chunk || !chunk->IsGenerated())
            continue;

        // Opaque part
        if (chunk->HasOpaqueMesh() && chunk->GetOpaqueMesh()) {
            ASCIIgL::Renderer::DrawCall dc;
            dc.mesh        = chunk->GetOpaqueMesh();
            dc.material    = mat.get();
            dc.layer       = 0;           // world geometry layer
            dc.transparent = false;       // opaque pass
            dc.sortKey     = 0.0f;        // not used for opaque

            renderer.SubmitDraw(dc);
            renderedCount++;
        }

        // Transparent part: single mesh per chunk, sorted at chunk level by view depth
        if (chunk->HasTransparentMesh() && chunk->GetTransparentMesh()) {
            glm::vec3 chunkCenter(
                chunk->GetCoord().x * Chunk::SIZE + Chunk::SIZE * 0.5f,
                chunk->GetCoord().y * Chunk::SIZE + Chunk::SIZE * 0.5f,
                chunk->GetCoord().z * Chunk::SIZE + Chunk::SIZE * 0.5f
            );
            glm::vec3 toChunk = chunkCenter - pos;
            float depth = glm::dot(toChunk, camDir); // positive = in front of camera

            ASCIIgL::Renderer::DrawCall dc;
            dc.mesh        = chunk->GetTransparentMesh();
            dc.material    = mat.get();
            dc.layer       = 0;           // world geometry layer
            dc.transparent = true;        // transparent pass
            dc.sortKey     = depth;       // back-to-front sorting using view depth

            renderer.SubmitDraw(dc);
            renderedCount++;
        }
    }

}

std::pair<uint32_t, WorldCoord> ChunkManager::BlockIntersectsView(glm::vec3& lookDir, glm::vec3& headPos, float reach) {
    glm::vec3 dir = glm::normalize(lookDir);
    const float step = 0.1f;

    float dist = 0.0f;
    while (dist <= reach) {
        glm::vec3 pos = headPos + dir * dist;

        int bx = static_cast<int>(floor(pos.x));
        int by = static_cast<int>(floor(pos.y));
        int bz = static_cast<int>(floor(pos.z));

        uint32_t stateId = GetBlockState(bx, by, bz);

        if (stateId != blockstate::BlockStateRegistry::AIR_STATE_ID) {
            return { stateId, WorldCoord(bx, by, bz) };
        }

        dist += step;
    }

    return { blockstate::BlockStateRegistry::AIR_STATE_ID, WorldCoord() };
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

        uint32_t stateId = GetBlockState(bx, by, bz);

        if (stateId == blockstate::BlockStateRegistry::AIR_STATE_ID) {
            lastEmpty = WorldCoord(bx, by, bz);
        }
        else {
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
