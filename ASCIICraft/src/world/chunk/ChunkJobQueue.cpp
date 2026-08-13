#include <ASCIICraft/world/chunk/ChunkJobQueue.hpp>

#include <algorithm>
#include <array>
#include <vector>
#include <cstring>

#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkMeshGen.hpp>
#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include <entt/entt.hpp>

#include <ASCIIgL/util/Logger.hpp>

namespace {

void CopyChunkBlocks(const Chunk* chunk, uint32_t* outBlocks) {
    if (!chunk || !outBlocks) return;
    std::memcpy(outBlocks, chunk->GetBlockData(), Chunk::VOLUME * sizeof(uint32_t));
}

} // namespace

ChunkJobQueue::ChunkJobQueue(entt::registry& registry)
    : ChunkJobQueue(registry, nullptr) {}

ChunkJobQueue::ChunkJobQueue(entt::registry& registry, std::unique_ptr<IChunkJobScheduler> scheduler)
    : registry_(registry)
    , scheduler_(scheduler ? std::move(scheduler) : MakeTbbChunkJobScheduler()) {}

ChunkJobQueue::~ChunkJobQueue() {
    // Drain before any member is destroyed. Tasks capture `this` and push into the
    // completed* queues, so they must not outlive them. scheduler_ is also declared
    // last (destroyed first) and waits again in its own destructor - both together
    // preserve the ordering the old inline task_group relied on.
    scheduler_->Wait();
}

void ChunkJobQueue::EnqueueTerrainGen(std::shared_ptr<Chunk> chunk) {
    if (!chunk) return;
    auto* bsr = registry_.ctx().find<blockstate::BlockStateRegistry>();
    if (!bsr) return;
    TerrainGenerator* gen = terrainGenerator_;
    const ChunkCoord coord = chunk->GetCoord();
    const uint64_t instanceId = chunk->GetInstanceId();

    // The lambda captures the shared_ptr by value, so the Chunk cannot be destroyed
    // while this job is queued or running. Previously it captured a raw pointer and an
    // unload could free the chunk underneath a worker mid-write.
    scheduler_->Run(ChunkJobTag{ ChunkJobKind::Terrain, coord },
                    [this, chunk = std::move(chunk), coord, instanceId, bsr, gen]() {
        // The chunk was unloaded before this job started. It is still alive (we hold a
        // reference) but nothing wants the result, so skip the work entirely.
        if (chunk->IsCancelled()) return;

        TerrainResult result;
        uint32_t* blocks = chunk->GetBlockDataForWrite();
        if (gen && bsr) {
            gen->GenerateChunkInto(coord, blocks, result, bsr);
        } else {
            std::fill(blocks, blocks + Chunk::VOLUME, 0u);
        }
        completedTerrainQueue_.push(CompletedTerrainResult{ coord, instanceId, std::move(result) });
    });
}

void ChunkJobQueue::EnqueueMeshGen(Chunk* chunk) {
    if (!chunk) return;
    auto* bsr = registry_.ctx().find<blockstate::BlockStateRegistry>();
    if (!bsr) return;
    ChunkCoord coord = chunk->GetCoord();

    auto chunkCopy = std::make_shared<std::vector<uint32_t>>(Chunk::VOLUME);
    CopyChunkBlocks(chunk, chunkCopy->data());

    auto neighborCopies = std::make_shared<std::array<std::vector<uint32_t>, 6>>();
    for (int i = 0; i < 6; ++i) {
        Chunk* neighbor = chunk->GetNeighbor(i);
        if (neighbor) {
            (*neighborCopies)[i].resize(Chunk::VOLUME);
            CopyChunkBlocks(neighbor, (*neighborCopies)[i].data());
        }
    }

    auto* modelLib = registry_.ctx().find<blockmodels::BlockModelLibrary>();
    if (!modelLib) {
        ASCIIgL::Logger::Warning("EnqueueMeshGen: BlockModelLibrary not found in context.");
        return;
    }

    scheduler_->Run(ChunkJobTag{ ChunkJobKind::Mesh, coord }, [this, coord, chunkCopy, neighborCopies, bsr, modelLib]() {
        std::array<const uint32_t*, 6> ptrs{};
        for (int i = 0; i < 6; ++i)
            ptrs[i] = (*neighborCopies)[i].empty() ? nullptr : (*neighborCopies)[i].data();
        ChunkMeshData data = BuildChunkMeshData(coord, chunkCopy->data(), ptrs, bsr, modelLib);
        completedMeshQueue_.push(CompletedMeshResult{ coord, std::move(data) });
    });
}

void ChunkJobQueue::DrainCompletedTerrainResultsInto(std::vector<CompletedTerrainResult>& out) {
    out.clear();
    CompletedTerrainResult result;
    while (completedTerrainQueue_.try_pop(result)) {
        out.push_back(std::move(result));
        if (maxDrainPerFrame_ > 0 && out.size() >= maxDrainPerFrame_) break;
    }
}

void ChunkJobQueue::DrainCompletedMeshResultsInto(std::vector<CompletedMeshResult>& out) {
    out.clear();
    CompletedMeshResult result;
    size_t limit = maxDrainMeshPerFrame_ != 0 ? maxDrainMeshPerFrame_ : maxDrainPerFrame_;
    while (completedMeshQueue_.try_pop(result)) {
        out.push_back(std::move(result));
        if (limit != 0 && out.size() >= limit) break;
    }
}

void ChunkJobQueue::EnqueueUnload(ChunkCoord coord, std::shared_ptr<Chunk> chunk, std::optional<MetaBucket> meta, bool closeRegionAfterSave, std::shared_ptr<RegionFile> region) {
    if (!chunk || !region) return;
    UnloadSaveCallback cb = unloadSaveCallback_;
    scheduler_->Run(ChunkJobTag{ ChunkJobKind::Unload, coord }, [cb, coord, chunk = std::move(chunk), meta = std::move(meta), closeRegionAfterSave, region = std::move(region)]() {
        if (cb && chunk && region)
            cb(chunk.get(), coord, meta ? &*meta : nullptr, closeRegionAfterSave, region);
    });
}

void ChunkJobQueue::WaitForPending() {
    scheduler_->Wait();
}
