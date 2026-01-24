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

// Forward declarations
class Player;

// Main World class
class World {
public:
    World(const WorldCoord& spawnPoint = WorldCoord(0, 10, 0));
    ~World();
    
    // Core world operations
    void Update();
    void Render();

    void SetPlayer(Player* player) { this->player = player; }
    Player* GetPlayer() const { return player; }
    
    // World queries
    WorldCoord GetSpawnPoint() const { return spawnPoint; }
    void SetSpawnPoint(const WorldCoord& pos) { spawnPoint = pos; }

private:
    WorldCoord spawnPoint;
    Player* player; // Reference to the current player for chunk streaming

    std::unique_ptr<ChunkManager> chunkManager;

};
