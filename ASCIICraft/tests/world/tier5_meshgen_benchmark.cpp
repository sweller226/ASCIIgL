// Chunk meshing threading benchmark.
//
// Measures the real wall-clock benefit of running BuildChunkMeshData across oneTBB
// worker threads (as ChunkJobQueue::EnqueueMeshGen does in production) versus running
// the identical per-chunk work serially on one thread. Not a correctness test - it
// exists to produce a number, not to assert behavior, so it is skipped by default.
//
// Run it explicitly:
//   ASCIICraft_tests.exe --test-case="*meshing threading benchmark*" --no-skip
// Build Release first; FastDebug timings are not representative.

#include <doctest/doctest.h>

#include "support/BlockRegistryFixture.hpp"

#include <ASCIICraft/world/Sizes.hpp>
#include <ASCIICraft/world/chunk/Chunk.hpp>
#include <ASCIICraft/world/chunk/ChunkMeshGen.hpp>
#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>
#include <ASCIICraft/world/terrain/TerrainResult.hpp>

#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using Blocks = std::vector<uint32_t>;

// Grid size tuned so the serial pass runs a few seconds in Release. Y band centered on
// the surface (see tier1_terrain_determinism.cpp's measured chunk-Y bands: 0-3
// underground, 4 transition, 5 surface, 6+ air) so the sample mixes solid, partial, and
// empty chunks like a real streamed area rather than one degenerate case.
constexpr int kGridX = 16;
constexpr int kGridZ = 16;
constexpr int kMinY = 3;
constexpr int kMaxY = 6; // inclusive
constexpr int kWarmupRuns = 1;
constexpr int kTimedRuns = 5;

/// A generated chunk's blocks plus a resolved pointer to each of its 6 neighbors'
/// blocks (nullptr at the sample's edge), mirroring Chunk::GetNeighbor / FaceDir order:
/// Top, Bottom, North(-Z), South(+Z), East(+X), West(-X).
struct SampleChunk {
    ChunkCoord coord;
    Blocks blocks;
    std::array<const uint32_t*, 6> neighbors{};
};

std::vector<SampleChunk> BuildSampleWorld() {
    entt::registry& reg = testsupport::SharedRegistry();
    TerrainGenerator gen(reg, /*worldSeed=*/2026ULL);
    const blockstate::BlockStateRegistry* bsr = &testsupport::SharedBlocks();

    std::vector<SampleChunk> chunks;
    chunks.reserve(static_cast<size_t>(kGridX) * kGridZ * (kMaxY - kMinY + 1));

    std::unordered_map<ChunkCoord, size_t> indexOf;
    for (int y = kMinY; y <= kMaxY; ++y) {
        for (int x = 0; x < kGridX; ++x) {
            for (int z = 0; z < kGridZ; ++z) {
                ChunkCoord coord{x, y, z};
                SampleChunk sc;
                sc.coord = coord;
                sc.blocks.assign(Chunk::VOLUME, 0u);
                TerrainResult result;
                gen.GenerateChunkInto(coord, sc.blocks.data(), result, bsr);
                indexOf[coord] = chunks.size();
                chunks.push_back(std::move(sc));
            }
        }
    }

    // Second pass: resolve neighbor pointers now that every chunk has a stable address.
    auto neighborCoord = [](ChunkCoord c, int dir) {
        switch (dir) {
            case 0: return ChunkCoord{c.x, c.y + 1, c.z}; // Top
            case 1: return ChunkCoord{c.x, c.y - 1, c.z}; // Bottom
            case 2: return ChunkCoord{c.x, c.y, c.z - 1}; // North (-Z)
            case 3: return ChunkCoord{c.x, c.y, c.z + 1}; // South (+Z)
            case 4: return ChunkCoord{c.x + 1, c.y, c.z}; // East (+X)
            default: return ChunkCoord{c.x - 1, c.y, c.z}; // West (-X)
        }
    };
    for (auto& sc : chunks) {
        for (int dir = 0; dir < 6; ++dir) {
            auto it = indexOf.find(neighborCoord(sc.coord, dir));
            sc.neighbors[dir] = (it != indexOf.end()) ? chunks[it->second].blocks.data() : nullptr;
        }
    }
    return chunks;
}

/// Mirrors ChunkJobQueue::EnqueueMeshGen's worker lambda: copy the chunk's block array
/// plus present neighbor arrays into fresh buffers, then mesh. The copy is genuinely
/// part of the threaded job's cost in production, so it belongs in the timed unit.
void RunMeshJob(const SampleChunk& sc,
                const blockstate::BlockStateRegistry* bsr,
                const blockmodels::BlockModelLibrary* models) {
    Blocks chunkCopy(sc.blocks);
    std::array<Blocks, 6> neighborCopies;
    std::array<const uint32_t*, 6> ptrs{};
    for (int i = 0; i < 6; ++i) {
        if (sc.neighbors[i]) {
            neighborCopies[i].assign(sc.neighbors[i], sc.neighbors[i] + Chunk::VOLUME);
            ptrs[i] = neighborCopies[i].data();
        }
    }
    ChunkMeshData data = BuildChunkMeshData(sc.coord, chunkCopy.data(), ptrs, bsr, models);
    // Touch the result so the optimizer cannot prove the buffers are dead and drop the work.
    volatile size_t sink = data.opaqueIndices.size() + data.transparentIndices.size();
    (void)sink;
}

double RunSerial(const std::vector<SampleChunk>& chunks,
                const blockstate::BlockStateRegistry* bsr,
                const blockmodels::BlockModelLibrary* models) {
    const auto start = std::chrono::steady_clock::now();
    for (const auto& sc : chunks) RunMeshJob(sc, bsr, models);
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double RunParallel(const std::vector<SampleChunk>& chunks,
                    const blockstate::BlockStateRegistry* bsr,
                    const blockmodels::BlockModelLibrary* models) {
    const auto start = std::chrono::steady_clock::now();
    oneapi::tbb::parallel_for(oneapi::tbb::blocked_range<size_t>(0, chunks.size()),
        [&](const oneapi::tbb::blocked_range<size_t>& range) {
            for (size_t i = range.begin(); i != range.end(); ++i) {
                RunMeshJob(chunks[i], bsr, models);
            }
        });
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double Average(const std::vector<double>& v) {
    double sum = 0.0;
    for (double x : v) sum += x;
    return sum / static_cast<double>(v.size());
}

} // namespace

TEST_SUITE("world.tier5.benchmark") {

TEST_CASE("meshing threading benchmark: serial vs oneTBB" * doctest::skip(true)) {
    const std::vector<SampleChunk> chunks = BuildSampleWorld();
    const blockstate::BlockStateRegistry* bsr = &testsupport::SharedBlocks();
    const blockmodels::BlockModelLibrary* models = &testsupport::SharedModels();

    for (int i = 0; i < kWarmupRuns; ++i) {
        RunSerial(chunks, bsr, models);
        RunParallel(chunks, bsr, models);
    }

    std::vector<double> serialMs, parallelMs;
    for (int i = 0; i < kTimedRuns; ++i) serialMs.push_back(RunSerial(chunks, bsr, models));
    for (int i = 0; i < kTimedRuns; ++i) parallelMs.push_back(RunParallel(chunks, bsr, models));

    const double serialAvg = Average(serialMs);
    const double parallelAvg = Average(parallelMs);
    const double speedup = serialAvg / parallelAvg;
    const double pctReduction = (serialAvg - parallelAvg) / serialAvg * 100.0;

    std::printf(
        "\n[meshing benchmark] chunks=%zu hardware_concurrency=%u\n"
        "  serial:   %.2f ms (avg of %d runs)\n"
        "  parallel: %.2f ms (avg of %d runs)\n"
        "  speedup:  %.2fx\n"
        "  reduction: %.1f%%\n",
        chunks.size(), std::thread::hardware_concurrency(),
        serialAvg, kTimedRuns, parallelAvg, kTimedRuns, speedup, pctReduction);

    CHECK(parallelAvg < serialAvg);
}

} // TEST_SUITE("world.tier5.benchmark")
