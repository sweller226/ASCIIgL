#include <ASCIICraft/world/World.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/Profiler.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>

#include <ASCIICraft/world/ChunkRegion.hpp>
#include <ASCIICraft/util/Util.hpp>

#include <entt/entt.hpp>

World::World(entt::registry& registry, const WorldCoord& spawnPoint)
    : spawnPoint(spawnPoint)
    , registry(registry) {
    ASCIIgL::Logger::Info("World created");
}

World::~World() {
    ASCIIgL::Logger::Info("World destroyed");
}

void World::Update() {
    {
        ASCIIgL::PROFILE_SCOPE("Update.ChunkManagement");
        chunkManager->Update();
    }
}

void World::Render() {
    {
        ASCIIgL::PROFILE_SCOPE("RenderWorld");
        chunkManager->RenderChunks();
    }
}

World* GetWorldPtr(entt::registry& registry) {
    if (!registry.ctx().contains<std::unique_ptr<World>>()) return nullptr;
    return registry.ctx().get<std::unique_ptr<World>>().get();
}