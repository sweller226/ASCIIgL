#include "support/WorldTestHarness.hpp"

#include "support/BlockRegistryFixture.hpp"

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/world/block/VanillaBlockRegistration.hpp>
#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/chunk/ChunkManagerDeps.hpp>

#include <memory>

namespace testsupport {

WorldTestHarness::WorldTestHarness(Config config)
    : config_(config)
    , dir_(config.label) {
    // Each harness needs its own registry - ChunkManager mutates the context and the
    // player entity must not be shared. buildLegacyRemapTable stays false because the
    // shared fixture registry owns that process-global table; a second builder would
    // corrupt v1 migration for everything else in the process.
    blockstate::VanillaRegistrationOptions opts;
    opts.buildLegacyRemapTable = false;
    blockstate::RegisterVanillaBlocksInContext(registry_, opts);

    player_ = registry_.create();
    registry_.emplace<ecs::components::PlayerTag>(player_);
    auto& transform = registry_.emplace<ecs::components::Transform>(player_);
    transform.position = config.playerPos;

    auto scheduler = std::make_unique<ManualChunkJobScheduler>();
    scheduler_ = scheduler.get();
    if (!config.dropMeshJobs) {
        scheduler_->SetPolicy(ChunkJobKind::Mesh, ManualChunkJobScheduler::Policy::Queue);
    }

    ChunkManagerDeps deps;
    deps.regionDir = dir_.Path();
    deps.nowSeconds = [this]() { return clock_; };
    deps.scheduler = std::move(scheduler);

    chunkManager_ = std::make_unique<ChunkManager>(
        registry_, config_.dims, config.renderDistance, config.seed, std::move(deps));

    referenceGenerator_ = std::make_unique<TerrainGenerator>(registry_, config.seed);
}

WorldTestHarness::~WorldTestHarness() = default;

void WorldTestHarness::MovePlayerTo(const glm::vec3& position) {
    auto& transform = registry_.get<ecs::components::Transform>(player_);
    transform.position = position;
}

void WorldTestHarness::MovePlayerToChunk(ChunkCoord coord) {
    // Centre of the chunk, so small float error cannot land in a neighbour.
    MovePlayerTo(glm::vec3(
        static_cast<float>(coord.x * sizes::CHUNK_SIZE + sizes::CHUNK_SIZE / 2),
        static_cast<float>(coord.y * sizes::CHUNK_SIZE + sizes::CHUNK_SIZE / 2),
        static_cast<float>(coord.z * sizes::CHUNK_SIZE + sizes::CHUNK_SIZE / 2)));
}

ChunkCoord WorldTestHarness::PlayerChunk() const {
    const auto& transform = registry_.get<ecs::components::Transform>(player_);
    return WorldCoord(glm::ivec3(transform.position)).ToChunkCoord();
}

void WorldTestHarness::Step(bool pumpJobs) {
    chunkManager_->Update();
    if (!pumpJobs) return;

    // Applying a terrain result enqueues mesh work and can buffer further edits, so a
    // single pump is not necessarily enough to reach a fixed point. Bounded to keep a
    // self-re-enqueueing job from spinning forever.
    for (int pass = 0; pass < 8 && scheduler_->PendingCount() > 0; ++pass) {
        scheduler_->RunAll();
        chunkManager_->Update();
    }
}

void WorldTestHarness::StepFrames(int count, bool pumpJobs) {
    for (int i = 0; i < count; ++i) Step(pumpJobs);
}

bool WorldTestHarness::Quiesce(int maxFrames) {
    for (int frame = 0; frame < maxFrames; ++frame) {
        Step(true);

        if (scheduler_->PendingCount() != 0) continue;

        bool allGenerated = true;
        for (const ChunkCoord coord : chunkManager_->GetLoadedCoords()) {
            const auto chunk = chunkManager_->GetChunkShared(coord);
            if (!chunk || !chunk->IsGenerated()) { allGenerated = false; break; }
        }
        if (allGenerated) return true;
    }
    return false;
}

std::vector<uint32_t> WorldTestHarness::GenerateReference(ChunkCoord coord,
                                                          TerrainResult* crossOut) {
    std::vector<uint32_t> blocks(Chunk::VOLUME, 0u);
    TerrainResult local;
    TerrainResult& result = crossOut ? *crossOut : local;
    referenceGenerator_->GenerateChunkInto(
        coord, blocks.data(), result,
        &registry_.ctx().get<blockstate::BlockStateRegistry>());
    return blocks;
}

std::vector<uint32_t> WorldTestHarness::BlocksOf(ChunkCoord coord) const {
    const auto chunk = chunkManager_->GetChunkShared(coord);
    if (!chunk) return {};
    const uint32_t* data = chunk->GetBlockData();
    return std::vector<uint32_t>(data, data + Chunk::VOLUME);
}

} // namespace testsupport
