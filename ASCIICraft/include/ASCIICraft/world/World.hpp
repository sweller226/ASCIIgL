#pragma once

#include <ASCIICraft/world/Block.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/TerrainGenerator.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/ChunkManager.hpp>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <mutex>
#include <queue>
#include <iterator>

#include <glm/glm.hpp>

#include <entt/entt.hpp>

// Main World class
class World {
public:
    World(entt::registry& registry, WorldCoord spawnPoint = WorldCoord(0, 90, 0), unsigned int renderDistance = 8);
    ~World();
    
    // Core world operations
    void Update();
    void Render();

    // World queries
    WorldCoord GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldCoord& pos) { spawnPoint = pos; }

    ChunkManager* GetChunkManager() const { return chunkManager.get(); };
    ChunkManager* GetChunkManager() { return chunkManager.get(); };


private:
    entt::registry& registry;
    WorldCoord spawnPoint;
    std::unique_ptr<ChunkManager> chunkManager;

    const unsigned int WORLD_LIMIT = 2048;
};

World* GetWorldPtr(entt::registry& registry);
