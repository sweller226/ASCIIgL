#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include "TerrainGenerator_Internal.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include <FastNoiseLite.h>

#include <ASCIICraft/world/Sizes.hpp>

using namespace terrain_generator_internal;

void TerrainGenerator::GenerateChunkInto(ChunkCoord coord, uint32_t* blocks, TerrainResult& result,
                                        const blockstate::BlockStateRegistry* bsr) {
    if (!bsr || !blocks) return;

    uint32_t airId = bsr->GetDefaultState("minecraft:air");
    const uint32_t waterDefaultId = bsr->GetDefaultState("minecraft:water");
    const uint32_t waterTopId = bsr->WithProperty(waterDefaultId, "top", "true");
    const uint32_t waterFullId = bsr->WithProperty(waterDefaultId, "top", "false");
    std::fill(blocks, blocks + Chunk::VOLUME, airId);

    InitializeNoiseGenerators();
    const TerrainParams params = GetTerrainParams();
    const int chunkBaseY = coord.y * sizes::CHUNK_SIZE;

    std::vector<glm::ivec3> treePlacementPositions;
    std::vector<glm::ivec3> flowerPlacementPositions;
    std::vector<glm::ivec3> grassPlacementPositions;
    std::vector<glm::ivec3> fernPlacementPositions;
    for (int x = 0; x < sizes::CHUNK_SIZE; ++x) {
        for (int z = 0; z < sizes::CHUNK_SIZE; ++z) {
            const glm::ivec3 worldCoord = LocalToWorldCoord(coord, x, z);
            const TerrainColumnSample sample = SampleTerrainColumn(worldCoord.x, worldCoord.z, params);
            for (int y = 0; y < sizes::CHUNK_SIZE; ++y) {
                const int worldY = chunkBaseY + y;
                const uint32_t stateId = GetBlockStateAt(
                    worldCoord.x, worldY, worldCoord.z, sample, params,
                    treePlacementPositions, flowerPlacementPositions, bsr, sample.dominantBiome
                );

                const int index = x + y * sizes::CHUNK_SIZE + z * sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
                if (stateId != airId) {
                    blocks[index] = stateId;
                } else if (worldY <= params.SEA_LEVEL) {
                    blocks[index] = (worldY == params.SEA_LEVEL) ? waterTopId : waterFullId;
                } else {
                    const float tarnNoise = (valleyNoise->GetNoise(
                        static_cast<float>(worldCoord.x) * 0.19f,
                        static_cast<float>(worldCoord.z) * 0.19f) + 1.0f) * 0.5f;
                    const bool highTarn =
                        sample.valleyFactor > 0.58f &&
                        sample.slope < 2.5f &&
                        sample.terrainHeight > params.SEA_LEVEL + 48 &&
                        tarnNoise > 0.57f;
                    if (highTarn && worldY == sample.terrainHeight + 1) {
                        blocks[index] = waterTopId;
                    }
                }
            }
        }
    }

    std::vector<WorldBlockPlacement> treeBlocks;
    for (const auto& pos : treePlacementPositions) {
        GenerateTreeInto(pos.x, pos.y, pos.z, bsr, treeBlocks);
    }

    const uint32_t grassId = bsr->GetDefaultState("minecraft:grass");

    auto blockKey = [](int x, int y, int z) -> uint64_t {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42) ^
               (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 21) ^
               static_cast<uint64_t>(static_cast<uint32_t>(z));
    };

    std::unordered_set<uint64_t> occupied;
    for (const auto& b : treeBlocks) {
        occupied.insert(blockKey(b.pos.x, b.pos.y, b.pos.z));
    }
    for (const auto& p : flowerPlacementPositions) {
        occupied.insert(blockKey(p.x, p.y, p.z));
    }

    auto canPlaceGrassAt = [&](int wx, int surfaceY, int wz) -> bool {
        if (occupied.count(blockKey(wx, surfaceY + 1, wz))) return false;
        const int lx = wx - coord.x * sizes::CHUNK_SIZE;
        const int lz = wz - coord.z * sizes::CHUNK_SIZE;
        const int surfaceLy = surfaceY - chunkBaseY;
        const int placeLy = surfaceLy + 1;
        if (lx < 0 || lx >= sizes::CHUNK_SIZE || lz < 0 || lz >= sizes::CHUNK_SIZE) return false;
        if (surfaceLy < 0 || placeLy >= sizes::CHUNK_SIZE) return false;
        const int i = lx + surfaceLy * sizes::CHUNK_SIZE + lz * sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
        return blocks[i] == grassId && blocks[i + sizes::CHUNK_SIZE] == airId;
    };

    // RandomPatch-like grass generation:
    // - pick random start in chunk (per patch)
    // - attempt many nearby offsets
    // - place if valid and biome-matching
    auto runGrassPatches = [&](BiomeType biome, int patchCount, int triesPerPatch, int xzSpread, int ySpread, float placeChance) {
        for (int patch = 0; patch < patchCount; ++patch) {
            const int startLocalX = static_cast<int>(RandomFloat(coord.x * 911 + patch * 43, coord.z * 673 + 17) * sizes::CHUNK_SIZE);
            const int startLocalZ = static_cast<int>(RandomFloat(coord.x * 421 + patch * 59, coord.z * 997 + 29) * sizes::CHUNK_SIZE);
            const int startWorldX = coord.x * sizes::CHUNK_SIZE + startLocalX;
            const int startWorldZ = coord.z * sizes::CHUNK_SIZE + startLocalZ;

            const TerrainColumnSample startSample = SampleTerrainColumn(startWorldX, startWorldZ, params);
            const BiomeWeights startWeights{
                startSample.flowerForestWeight,
                startSample.forestWeight
            };
            const float startWeight = BiomeWeightFor(biome, startWeights);
            if (startWeight < 0.22f) continue;

            const int startSurfaceY = startSample.terrainHeight;
            if (startSurfaceY < params.SEA_LEVEL) continue;

            for (int attempt = 0; attempt < triesPerPatch; ++attempt) {
                const int offX = static_cast<int>(RandomFloat(startWorldX + attempt * 31, startWorldZ - attempt * 17) * (2 * xzSpread + 1)) - xzSpread;
                const int offZ = static_cast<int>(RandomFloat(startWorldX - attempt * 19, startWorldZ + attempt * 37) * (2 * xzSpread + 1)) - xzSpread;


                const int wx = startWorldX + offX;
                const int wz = startWorldZ + offZ;
                const TerrainColumnSample placementSample = SampleTerrainColumn(wx, wz, params);
                const BiomeWeights placementWeights{
                    placementSample.flowerForestWeight,
                    placementSample.forestWeight
                };
                const float placementWeight = BiomeWeightFor(biome, placementWeights);
                if (placementWeight < 0.22f) continue;

                const int surfaceY = placementSample.terrainHeight;
                if (surfaceY < params.SEA_LEVEL) continue;
                if (placementSample.slope > 5.0f) continue;
                if (RandomFloat(wx + patch * 13 + attempt * 7, wz - patch * 5 + attempt * 11) > placeChance * placementWeight) continue;
                if (!canPlaceGrassAt(wx, surfaceY, wz)) continue;

                grassPlacementPositions.push_back(glm::ivec3(wx, surfaceY + 1, wz));
            }
        }
    };

    runGrassPatches(BiomeType::FlowerForest, 3, 16, 5, 1, 0.30f);
    for (const auto& pos : grassPlacementPositions) {
        occupied.insert(blockKey(pos.x, pos.y, pos.z));
    }

    auto runFernPatches = [&](BiomeType biome, int patchCount, int triesPerPatch, int xzSpread, float placeChance,
                              float tallGrassMix = 0.0f) {
        for (int patch = 0; patch < patchCount; ++patch) {
            const int startLocalX = static_cast<int>(RandomFloat(coord.x * 1237 + patch * 53, coord.z * 809 + 23) * sizes::CHUNK_SIZE);
            const int startLocalZ = static_cast<int>(RandomFloat(coord.x * 557 + patch * 61, coord.z * 1103 + 31) * sizes::CHUNK_SIZE);
            const int startWorldX = coord.x * sizes::CHUNK_SIZE + startLocalX;
            const int startWorldZ = coord.z * sizes::CHUNK_SIZE + startLocalZ;

            const TerrainColumnSample startSample = SampleTerrainColumn(startWorldX, startWorldZ, params);
            const BiomeWeights startWeights{
                startSample.flowerForestWeight,
                startSample.forestWeight
            };
            const float startWeight = BiomeWeightFor(biome, startWeights);
            if (startWeight < 0.22f) continue;

            const int startSurfaceY = startSample.terrainHeight;
            if (startSurfaceY < params.SEA_LEVEL) continue;

            for (int attempt = 0; attempt < triesPerPatch; ++attempt) {
                const int offX = static_cast<int>(RandomFloat(startWorldX + attempt * 37, startWorldZ - attempt * 19) * (2 * xzSpread + 1)) - xzSpread;
                const int offZ = static_cast<int>(RandomFloat(startWorldX - attempt * 23, startWorldZ + attempt * 41) * (2 * xzSpread + 1)) - xzSpread;

                const int wx = startWorldX + offX;
                const int wz = startWorldZ + offZ;
                const TerrainColumnSample placementSample = SampleTerrainColumn(wx, wz, params);
                const BiomeWeights placementWeights{
                    placementSample.flowerForestWeight,
                    placementSample.forestWeight
                };
                const float placementWeight = BiomeWeightFor(biome, placementWeights);
                if (placementWeight < 0.22f) continue;

                const int surfaceY = placementSample.terrainHeight;
                if (surfaceY < params.SEA_LEVEL) continue;
                if (placementSample.slope > 5.0f) continue;
                if (RandomFloat(wx + patch * 17 + attempt * 9, wz - patch * 7 + attempt * 13) > placeChance * placementWeight) continue;
                if (!canPlaceGrassAt(wx, surfaceY, wz)) continue;

                const glm::ivec3 placePos(wx, surfaceY + 1, wz);
                const bool useTallGrass =
                    tallGrassMix > 0.0f &&
                    RandomFloat(wx - patch * 3 + attempt * 5, wz + patch * 7 - attempt * 11) < tallGrassMix;
                if (useTallGrass) {
                    grassPlacementPositions.push_back(placePos);
                } else {
                    fernPlacementPositions.push_back(placePos);
                }
                occupied.insert(blockKey(placePos.x, placePos.y, placePos.z));
            }
        }
    };

    runFernPatches(BiomeType::Forest, 8, 20, 5, 0.62f, 0.5f);
    runFernPatches(BiomeType::FlowerForest, 2, 10, 4, 0.20f);

    const uint32_t tallGrassId = bsr->GetDefaultState("minecraft:tall_grass");
    const uint32_t fernId = bsr->GetDefaultState("minecraft:fern");
    std::unordered_set<uint64_t> seenGroundCover;
    for (const auto& pos : grassPlacementPositions) {
        const uint64_t key =
            (static_cast<uint64_t>(static_cast<uint32_t>(pos.x)) << 42) ^
            (static_cast<uint64_t>(static_cast<uint32_t>(pos.y)) << 21) ^
            static_cast<uint64_t>(static_cast<uint32_t>(pos.z));
        if (!seenGroundCover.insert(key).second) continue;
        result.crossChunkBlocks.push_back(WorldBlockPlacement{ WorldCoord(pos.x, pos.y, pos.z), tallGrassId });
    }
    for (const auto& pos : fernPlacementPositions) {
        const uint64_t key =
            (static_cast<uint64_t>(static_cast<uint32_t>(pos.x)) << 42) ^
            (static_cast<uint64_t>(static_cast<uint32_t>(pos.y)) << 21) ^
            static_cast<uint64_t>(static_cast<uint32_t>(pos.z));
        if (!seenGroundCover.insert(key).second) continue;
        result.crossChunkBlocks.push_back(WorldBlockPlacement{ WorldCoord(pos.x, pos.y, pos.z), fernId });
    }

    const uint32_t poppyId = bsr->GetDefaultState("minecraft:poppy");
    const uint32_t dandelionId = bsr->GetDefaultState("minecraft:dandelion");
    for (const auto& pos : flowerPlacementPositions) {
        const float flowerNoise = treeNoise->GetNoise(static_cast<float>(pos.x), static_cast<float>(pos.z));
        const uint32_t flowerId = (flowerNoise > 0.0f) ? poppyId : dandelionId;
        result.crossChunkBlocks.push_back(WorldBlockPlacement{ WorldCoord(pos.x, pos.y, pos.z), flowerId });
    }
    result.crossChunkBlocks.insert(result.crossChunkBlocks.end(), treeBlocks.begin(), treeBlocks.end());
}

glm::ivec3 TerrainGenerator::LocalToWorldCoord(const ChunkCoord& coord, int localX, int localZ) const {
    return glm::ivec3(
        coord.x * sizes::CHUNK_SIZE + localX,
        0, // Y will be calculated per-block
        coord.z * sizes::CHUNK_SIZE + localZ
    );
}
