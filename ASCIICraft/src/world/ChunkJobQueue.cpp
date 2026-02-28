#include <ASCIICraft/world/ChunkJobQueue.hpp>
#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/ChunkMeshGen.hpp>
#include <ASCIICraft/world/TerrainGenerator.hpp>

#include <array>
#include <vector>
#include <cstring>

#include <entt/entt.hpp>

namespace {

void CopyChunkBlocks(const Chunk* chunk, uint32_t* outBlocks) {
    if (!chunk || !outBlocks) return;
    std::memcpy(outBlocks, chunk->GetBlockData(), Chunk::VOLUME * sizeof(uint32_t));
}

void GenerateTerrainIntoBuffer(
    ChunkCoord coord,
    TerrainResult& result,
    const blockstate::BlockStateRegistry* bsr,
    TerrainGenerator* terrainGen
) {
    if (terrainGen && bsr)
        terrainGen->GenerateChunkInto(coord, result, bsr);
    else {
        result.blocks.resize(Chunk::VOLUME);
        std::fill(result.blocks.begin(), result.blocks.end(), 0u);
    }
}

} // namespace

ChunkJobQueue::ChunkJobQueue(entt::registry& registry)
    : registry_(registry) {}

ChunkJobQueue::~ChunkJobQueue() {
    WaitForPending();
}

void ChunkJobQueue::EnqueueTerrainGen(Chunk* chunk) {
    if (!chunk) return;
    auto* bsr = registry_.ctx().find<blockstate::BlockStateRegistry>();
    if (!bsr) return;
    TerrainGenerator* gen = terrainGenerator_;
    ChunkCoord coord = chunk->GetCoord();
    taskGroup_.run([this, coord, bsr, gen]() {
        TerrainResult result;
        GenerateTerrainIntoBuffer(coord, result, bsr, gen);
        completedTerrainQueue_.push(CompletedTerrainResult{ coord, std::move(result) });
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

    taskGroup_.run([this, coord, chunkCopy, neighborCopies, bsr]() {
        std::array<const uint32_t*, 6> ptrs{};
        for (int i = 0; i < 6; ++i)
            ptrs[i] = (*neighborCopies)[i].empty() ? nullptr : (*neighborCopies)[i].data();
        ChunkMeshData data = BuildChunkMeshData(coord, chunkCopy->data(), ptrs, bsr);
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

void ChunkJobQueue::WaitForPending() {
    taskGroup_.wait();
}
