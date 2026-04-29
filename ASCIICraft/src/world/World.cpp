#include <ASCIICraft/world/World.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Material.hpp>

#include <ASCIICraft/world/chunk/ChunkRegion.hpp>
#include <ASCIICraft/util/Util.hpp>

#include <entt/entt.hpp>

World::World(entt::registry& registry, WorldParams params)
    : registry(registry)
    , spawnPoint(params.spawnPoint)
    , worldSeed(params.worldSeed) {
    ASCIIgL::Logger::Info("World created");

    chunkManager = std::make_unique<ChunkManager>(registry, worldDimensions, params.renderDistance, params.worldSeed);
}

World::~World() {
    ASCIIgL::Logger::Info("World destroyed");
}

void World::Update() {
    {
        PROFILE_SCOPE("Update.ChunkManagement");
        chunkManager->Update();
    }
}

void World::Render() {
    {
        PROFILE_SCOPE("RenderWorld");
        chunkManager->RenderChunks();
    }
}

World* GetWorldPtr(entt::registry& registry) {
    std::unique_ptr<World>* worldPtr = registry.ctx().find<std::unique_ptr<World>>();

    if (!worldPtr) {
        ASCIIgL::Logger::Error("World not found in registry context!");
        return nullptr;
    }

    if (!worldPtr->get()) {
        ASCIIgL::Logger::Error("World pointer exists but is NULL!");
        return nullptr;
    }

    World* world = worldPtr->get();

    return world;
}