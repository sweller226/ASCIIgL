#pragma once

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/chunk/ChunkManager.hpp>
#include <ASCIICraft/world/Sizes.hpp>

#include <cstdint>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <iterator>

#include <glm/glm.hpp>

#include <entt/entt.hpp>

/// Construction-time settings for \ref World (spawn, streaming, procedural seed).
struct WorldParams {
    WorldCoord spawnPoint = WorldCoord(0, 90, 0);
    unsigned int renderDistance = 8;
    /// Master seed for terrain and other derived world RNG; must be fixed for a save.
    uint64_t worldSeed = 12345ULL;
};

// Main World class
class World {
public:
    explicit World(entt::registry& registry, WorldParams params = {});
    ~World();
    
    // Core world operations
    void Update();
    void Render();

    // World queries
    WorldCoord GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldCoord& pos) { spawnPoint = pos; }

    uint64_t GetWorldSeed() const { return worldSeed; }

    ChunkManager* GetChunkManager() const { return chunkManager.get(); };
    ChunkManager* GetChunkManager() { return chunkManager.get(); };


private:
    entt::registry& registry;
    WorldCoord spawnPoint;
    uint64_t worldSeed;
    std::unique_ptr<ChunkManager> chunkManager;

    const sizes::WorldDimensions worldDimensions = sizes::WorldDimensions(1024, 0, 1024);
};

World* GetWorldPtr(entt::registry& registry);
