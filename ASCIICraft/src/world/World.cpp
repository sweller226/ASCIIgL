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

World::World(const WorldCoord& spawnPoint)
    : spawnPoint(spawnPoint)
    , player(nullptr) {
    ASCIIgL::Logger::Info("World created");
}

World::~World() {
    ASCIIgL::Logger::Info("World destroyed");
}

void World::Update() {
    if (!player) {
        return;
    }
    
    {
        ASCIIgL::PROFILE_SCOPE("Update.ChunkManagement");
        chunkManager->Update();
    }
}

void World::Render() {
    if (!player) {
        ASCIIgL::Logger::Warning("No player set for world rendering");
        return;
    }

    std::vector<Chunk*> visibleChunks = GetVisibleChunks(player->GetPosition(), player->GetCamera().getCamFront());
    ASCIIgL::Logger::Debug("Render: visibleChunks = " + std::to_string(visibleChunks.size()));

    // Set up view-projection matrix once
    glm::mat4 mvp = player->GetCamera().proj * player->GetCamera().view * glm::mat4(1.0f);
    auto mat = ASCIIgL::MaterialLibrary::GetInst().GetDefault();
    ASCIIgL::RendererGPU::GetInst().BindMaterial(mat.get());
    mat->SetMatrix4("mvp", mvp);
    ASCIIgL::RendererGPU::GetInst().UploadMaterialConstants(mat.get());
    
    // Render each chunk individually - leverages GPU mesh caching
    for (Chunk* chunk : visibleChunks) {
        if (!chunk || !chunk->IsGenerated()) {
            continue;
        }
        chunk->Render();
    }
}