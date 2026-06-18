#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <unordered_set>

#include <FastNoiseLite/FastNoiseLite.h>

#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>
#include <ASCIICraft/world/Sizes.hpp>

namespace {

/// Deterministic per-layer seed from one master world seed (\p salt distinguishes noise streams).
inline int DerivedNoiseSeed(uint64_t worldSeed, uint64_t salt) {
    uint64_t x = worldSeed + 0x9e3779b97f4a7c15ULL + (salt << 6) + (salt >> 2);
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return static_cast<int>(x & 0x7fffffff);
}
constexpr uint64_t kSaltTerrain = 1;
constexpr uint64_t kSaltForestDensity = 2;
constexpr uint64_t kSaltTree = 3;
constexpr uint64_t kSaltFlowerDensity = 4;
constexpr uint64_t kSaltBiomeTemperature = 5;
constexpr uint64_t kSaltBiomeHumidity = 6;
constexpr uint64_t kSaltSurfaceDepth = 7;

float SmoothStep(float edge0, float edge1, float x) {
    if (edge0 >= edge1) {
        return x >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct BiomeWeights {
    float plains;
    float flowerForest;
    float forest;
};

BiomeWeights ComputeBiomeWeights(float temperature, float humidity) {
    const float plainsW = 1.0f - SmoothStep(0.37f, 0.45f, humidity);
    const float flowerHumidW = SmoothStep(0.54f, 0.68f, humidity);
    const float flowerTempW = SmoothStep(0.28f, 0.42f, temperature) * (1.0f - SmoothStep(0.70f, 0.76f, temperature));
    const float flowerW = flowerHumidW * flowerTempW;
    const float forestW = std::max(0.0f, 1.0f - plainsW - flowerW);
    const float sum = plainsW + flowerW + forestW;
    if (sum < 1e-6f) {
        return {0.0f, 0.0f, 1.0f};
    }
    return {plainsW / sum, flowerW / sum, forestW / sum};
}

BiomeType DominantBiome(const BiomeWeights& weights) {
    if (weights.plains >= weights.flowerForest && weights.plains >= weights.forest) {
        return BiomeType::Plains;
    }
    if (weights.flowerForest >= weights.forest) {
        return BiomeType::FlowerForest;
    }
    return BiomeType::Forest;
}

float BiomeWeightFor(BiomeType biome, const BiomeWeights& weights) {
    switch (biome) {
        case BiomeType::Plains: return weights.plains;
        case BiomeType::FlowerForest: return weights.flowerForest;
        case BiomeType::Forest: return weights.forest;
        default: return weights.forest;
    }
}

} // namespace
float RandomFloat(int x, int z) {
    uint32_t seed = x * 374761393u + z * 668265263u; 
    seed = (seed ^ (seed >> 13)) * 1274126177u;
    seed ^= (seed >> 16);
    return (seed & 0xFFFFFF) / float(0xFFFFFF);
}
TerrainGenerator::TerrainGenerator(entt::registry &registry, uint64_t worldSeed)
    : m_registry(registry)
    , m_worldSeed(worldSeed) {
    terrainNoise = std::make_unique<FastNoiseLite>();
    treeNoise = std::make_unique<FastNoiseLite>();
    forestDensityNoise = std::make_unique<FastNoiseLite>();
    flowerDensityNoise = std::make_unique<FastNoiseLite>();
    biomeTemperatureNoise = std::make_unique<FastNoiseLite>();
    biomeHumidityNoise = std::make_unique<FastNoiseLite>();
    surfaceDepthNoise = std::make_unique<FastNoiseLite>();

    if (registry.ctx().contains<blockstate::BlockStateRegistry>()) {
        m_bsr = &registry.ctx().get<blockstate::BlockStateRegistry>();
    } else {
        ASCIIgL::Logger::Warning("BlockStateRegistry missing from context");
        m_bsr = nullptr;
    }
}   
BiomeType TerrainGenerator::GetBiomeType(int worldx, int worldz) {
    const float t = SampleBiomeTemperature(worldx, worldz);
    const float h = SampleBiomeHumidity(worldx, worldz);
    return DominantBiome(ComputeBiomeWeights(t, h));
}
TerrainGenerator::~TerrainGenerator() = default;

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
            const BiomeType biomeType = GetBiomeType(worldCoord.x, worldCoord.z);
            const int terrainHeight = CalculateTerrainHeight(worldCoord.x, worldCoord.z, params, biomeType);
            for (int y = 0; y < sizes::CHUNK_SIZE; ++y) {
                const int worldY = chunkBaseY + y;
                const uint32_t stateId = GetBlockStateAt(
                    worldCoord.x, worldY, worldCoord.z, terrainHeight, params,
                    treePlacementPositions, flowerPlacementPositions, bsr, biomeType
                );

                const int index = x + y * sizes::CHUNK_SIZE + z * sizes::CHUNK_SIZE * sizes::CHUNK_SIZE;
                if (stateId != airId) {
                    blocks[index] = stateId;
                } else if (worldY <= params.SEA_LEVEL) {
                    blocks[index] = (worldY == params.SEA_LEVEL) ? waterTopId : waterFullId;
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

            const BiomeType startBiome = GetBiomeType(startWorldX, startWorldZ);
            const float startWeight = BiomeWeightFor(biome, ComputeBiomeWeights(
                SampleBiomeTemperature(startWorldX, startWorldZ),
                SampleBiomeHumidity(startWorldX, startWorldZ)));
            if (startWeight < 0.22f) continue;

            const int startSurfaceY = CalculateTerrainHeight(startWorldX, startWorldZ, params, startBiome);
            if (startSurfaceY < params.SEA_LEVEL) continue;

            for (int attempt = 0; attempt < triesPerPatch; ++attempt) {
                const int offX = static_cast<int>(RandomFloat(startWorldX + attempt * 31, startWorldZ - attempt * 17) * (2 * xzSpread + 1)) - xzSpread;
                const int offZ = static_cast<int>(RandomFloat(startWorldX - attempt * 19, startWorldZ + attempt * 37) * (2 * xzSpread + 1)) - xzSpread;
                

                const int wx = startWorldX + offX;
                const int wz = startWorldZ + offZ;
                const BiomeType placementBiome = GetBiomeType(wx, wz);
                const float placementWeight = BiomeWeightFor(biome, ComputeBiomeWeights(
                    SampleBiomeTemperature(wx, wz),
                    SampleBiomeHumidity(wx, wz)));
                if (placementWeight < 0.22f) continue;

                const int surfaceY = CalculateTerrainHeight(wx, wz, params, placementBiome);
                if (surfaceY < params.SEA_LEVEL) continue;
                if (RandomFloat(wx + patch * 13 + attempt * 7, wz - patch * 5 + attempt * 11) > placeChance * placementWeight) continue;
                if (!canPlaceGrassAt(wx, surfaceY, wz)) continue;

                grassPlacementPositions.push_back(glm::ivec3(wx, surfaceY + 1, wz));
            }
        }
    };

    runGrassPatches(BiomeType::Plains, 12, 32, 7, 1, 0.74f);
    runGrassPatches(BiomeType::FlowerForest, 4, 18, 5, 1, 0.34f);
    runGrassPatches(BiomeType::Forest, 2, 12, 4, 1, 0.20f);
    for (const auto& pos : grassPlacementPositions) {
        occupied.insert(blockKey(pos.x, pos.y, pos.z));
    }

    auto runFernPatches = [&](BiomeType biome, int patchCount, int triesPerPatch, int xzSpread, float placeChance) {
        for (int patch = 0; patch < patchCount; ++patch) {
            const int startLocalX = static_cast<int>(RandomFloat(coord.x * 1237 + patch * 53, coord.z * 809 + 23) * sizes::CHUNK_SIZE);
            const int startLocalZ = static_cast<int>(RandomFloat(coord.x * 557 + patch * 61, coord.z * 1103 + 31) * sizes::CHUNK_SIZE);
            const int startWorldX = coord.x * sizes::CHUNK_SIZE + startLocalX;
            const int startWorldZ = coord.z * sizes::CHUNK_SIZE + startLocalZ;

            const BiomeType startBiome = GetBiomeType(startWorldX, startWorldZ);
            const float startWeight = BiomeWeightFor(biome, ComputeBiomeWeights(
                SampleBiomeTemperature(startWorldX, startWorldZ),
                SampleBiomeHumidity(startWorldX, startWorldZ)));
            if (startWeight < 0.22f) continue;

            const int startSurfaceY = CalculateTerrainHeight(startWorldX, startWorldZ, params, startBiome);
            if (startSurfaceY < params.SEA_LEVEL) continue;

            for (int attempt = 0; attempt < triesPerPatch; ++attempt) {
                const int offX = static_cast<int>(RandomFloat(startWorldX + attempt * 37, startWorldZ - attempt * 19) * (2 * xzSpread + 1)) - xzSpread;
                const int offZ = static_cast<int>(RandomFloat(startWorldX - attempt * 23, startWorldZ + attempt * 41) * (2 * xzSpread + 1)) - xzSpread;

                const int wx = startWorldX + offX;
                const int wz = startWorldZ + offZ;
                const BiomeType placementBiome = GetBiomeType(wx, wz);
                const float placementWeight = BiomeWeightFor(biome, ComputeBiomeWeights(
                    SampleBiomeTemperature(wx, wz),
                    SampleBiomeHumidity(wx, wz)));
                if (placementWeight < 0.22f) continue;

                const int surfaceY = CalculateTerrainHeight(wx, wz, params, placementBiome);
                if (surfaceY < params.SEA_LEVEL) continue;
                if (RandomFloat(wx + patch * 17 + attempt * 9, wz - patch * 7 + attempt * 13) > placeChance * placementWeight) continue;
                if (!canPlaceGrassAt(wx, surfaceY, wz)) continue;

                fernPlacementPositions.push_back(glm::ivec3(wx, surfaceY + 1, wz));
            }
        }
    };

    runFernPatches(BiomeType::Forest, 6, 18, 5, 0.54f);
    runFernPatches(BiomeType::FlowerForest, 2, 10, 4, 0.24f);

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

int RandomRange(int x, int z, int lo, int hi) {
    if (lo >= hi) return lo;
    const int span = hi - lo + 1;
    return lo + std::min(span - 1, static_cast<int>(RandomFloat(x, z) * span));
}

using TreePushFn = std::function<void(int, int, int, uint32_t)>;

void PlaceLeafBlob(int cx, int cy, int cz, int radius, int layers, bool skipCenterOnLowerDiscs,
                   uint32_t leavesId, const TreePushFn& push) {
    for (int dy = 0; dy < layers; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (std::abs(dx) == radius && std::abs(dz) == radius) continue;
                // When enabled, keep a hole in the center so logs aren't overwritten by leaves.
                // (Old oak blob logic always skipped dx==0 && dz==0 for disc layers.)
                if (skipCenterOnLowerDiscs && dx == 0 && dz == 0) continue;
                push(cx + dx, cy + dy, cz + dz, leavesId);
            }
        }
    }
}

void GenerateOakBlob(int worldX, int worldY, int worldZ, int trunkHeight, int leafRadius, int leafDiscLayers,
                   const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    if (!bsr) return;

    const uint32_t dirtId = bsr->GetDefaultState("minecraft:dirt");
    const uint32_t logId = bsr->GetDefaultState("minecraft:oak_log");
    const uint32_t leavesId = bsr->GetDefaultState("minecraft:oak_leaves");

    TreePushFn push = [&out](int x, int y, int z, uint32_t stateId) {
        out.push_back(WorldBlockPlacement{ WorldCoord(x, y, z), stateId });
    };

    push(worldX, worldY - 1, worldZ, dirtId);
    for (int i = 0; i < trunkHeight; ++i) {
        push(worldX, worldY + i, worldZ, logId);
    }

    const int leafBaseY = worldY + trunkHeight - 3;
    PlaceLeafBlob(worldX, leafBaseY, worldZ, leafRadius, leafDiscLayers, true, leavesId, push);

    const int ringY = leafBaseY + leafDiscLayers;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            if (dx == 0 && dz == 0) continue;
            push(worldX + dx, ringY, worldZ + dz, leavesId);
        }
    }
    const int crownY = ringY + 1;
    push(worldX, crownY, worldZ, leavesId);
    push(worldX, crownY, worldZ + 1, leavesId);
    push(worldX, crownY, worldZ - 1, leavesId);
    push(worldX + 1, crownY, worldZ, leavesId);
    push(worldX - 1, crownY, worldZ, leavesId);
}

 

void GenerateTreeForBiome(BiomeType biome, int worldX, int worldY, int worldZ,
                          const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    const float roll = RandomFloat(worldX, worldZ);

    switch (biome) {
        case BiomeType::Plains:
            if (roll < 0.60f) {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX, worldZ, 4, 5), 1, 1, bsr, out);
            } else if (roll < 0.95f) {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX + 3, worldZ - 5, 5, 6),
                    RandomRange(worldX - 7, worldZ + 9, 1, 2),
                    RandomRange(worldX + 11, worldZ - 13, 1, 2), bsr, out);
            } else {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX + 19, worldZ + 21, 6, 7), 2, 2, bsr, out);
            }
            break;

        case BiomeType::FlowerForest:
            if (roll < 0.25f) {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX, worldZ, 5, 6),
                    RandomRange(worldX + 5, worldZ - 3, 1, 2),
                    RandomRange(worldX - 9, worldZ + 7, 1, 2), bsr, out);
            } else if (roll < 0.75f) {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX + 13, worldZ - 17, 6, 7), 2,
                    RandomRange(worldX - 21, worldZ + 23, 2, 3), bsr, out);
            } else {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX + 27, worldZ - 29, 7, 8),
                    RandomRange(worldX - 31, worldZ + 33, 2, 3), 3, bsr, out);
            }
            break;

        case BiomeType::Forest:
            if (roll < 0.20f) {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX, worldZ, 6, 7), 2,
                    RandomRange(worldX + 5, worldZ - 7, 2, 3), bsr, out);
            } else if (roll < 0.65f) {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX + 11, worldZ - 13, 7, 9),
                    RandomRange(worldX - 15, worldZ + 17, 2, 3),
                    RandomRange(worldX + 19, worldZ - 21, 3, 4), bsr, out);
            } else {
                GenerateOakBlob(worldX, worldY, worldZ,
                    RandomRange(worldX + 23, worldZ - 25, 8, 10),
                    RandomRange(worldX - 27, worldZ + 29, 2, 3),
                    RandomRange(worldX + 31, worldZ - 33, 3, 4), bsr, out);
            }
            break;

        default:
            break;
    }
}

void TerrainGenerator::GenerateTreeInto(int worldX, int worldY, int worldZ,
                                        const blockstate::BlockStateRegistry* bsr,
                                        std::vector<WorldBlockPlacement>& out) {
    GenerateTreeForBiome(GetBiomeType(worldX, worldZ), worldX, worldY, worldZ, bsr, out);
}

glm::ivec3 TerrainGenerator::LocalToWorldCoord(const ChunkCoord& coord, int localX, int localZ) const {
    return glm::ivec3(
        coord.x * sizes::CHUNK_SIZE + localX,
        0, // Y will be calculated per-block
        coord.z * sizes::CHUNK_SIZE + localZ
    );
}

uint32_t TerrainGenerator::GetBlockStateAt(int worldX, int worldY, int worldZ, int terrainHeight,
                                          const TerrainParams& params,
                                          std::vector<glm::ivec3>& treePlacementPositions,
                                          std::vector<glm::ivec3>& flowerPlacementPositions,
                                          const blockstate::BlockStateRegistry* bsrOverride, BiomeType biomeType) {
    const blockstate::BlockStateRegistry* bsr = bsrOverride ? bsrOverride : m_bsr;
    if (!bsr) return 0u;
    uint32_t bedrockId = bsr->GetDefaultState("minecraft:bedrock");
    uint32_t airId = bsr->GetDefaultState("minecraft:air");

    // Bedrock layer at world bottom
    if (worldY == 0) {
        return bedrockId;
    }
    
    // Air above terrain
    if (worldY > terrainHeight) {
        return airId;
    }
    
    const int depthFromSurface = terrainHeight - worldY;
    return DetermineBlockState(
        worldX, worldY, worldZ, depthFromSurface, params, treePlacementPositions, flowerPlacementPositions, bsrOverride, biomeType
    );
}

void TerrainGenerator::InitializeNoiseGenerators() {
    std::call_once(noiseInitFlag, [this]() {
    terrainNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    terrainNoise->SetFrequency(0.018f);
    terrainNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise->SetFractalOctaves(4);
    terrainNoise->SetFractalLacunarity(2.0f);
    terrainNoise->SetFractalGain(0.5f);
    terrainNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltTerrain));

    // Forest density noise
    forestDensityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    forestDensityNoise->SetFrequency(0.01f);
    forestDensityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    forestDensityNoise->SetFractalOctaves(4);
    forestDensityNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltForestDensity));

    // Flower density noise 
    flowerDensityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    flowerDensityNoise->SetFrequency(0.01f);
    flowerDensityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    flowerDensityNoise->SetFractalOctaves(3);
    flowerDensityNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltFlowerDensity));

    // Biome climate noises (large, smooth regions)
    biomeTemperatureNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    biomeTemperatureNoise->SetFrequency(0.0025f);
    biomeTemperatureNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeTemperatureNoise->SetFractalOctaves(3);
    biomeTemperatureNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltBiomeTemperature));

    biomeHumidityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    biomeHumidityNoise->SetFrequency(0.0022f);
    biomeHumidityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeHumidityNoise->SetFractalOctaves(3);
    biomeHumidityNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltBiomeHumidity));

    // Surface depth noise used for dirt thickness variation.
    surfaceDepthNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    surfaceDepthNoise->SetFrequency(0.015f);
    surfaceDepthNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    surfaceDepthNoise->SetFractalOctaves(2);
    surfaceDepthNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltSurfaceDepth));

    treeNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    treeNoise->SetFrequency(0.25f);
    treeNoise->SetFractalType(FastNoiseLite::FractalType_None);
    treeNoise->SetSeed(DerivedNoiseSeed(m_worldSeed, kSaltTree));
    });
}

TerrainGenerator::TerrainParams TerrainGenerator::GetTerrainParams() const {
    TerrainParams params;
    params.BASE_HEIGHT = 5;
    params.MIN_TERRAIN_HEIGHT = 3;
    params.MAX_TERRAIN_HEIGHT = 8;
    params.AMPLITUDE = 1.65f;
    params.DIRT_DEPTH = 3;
    params.SEA_LEVEL = 75;
    return params;
}

float TerrainGenerator::SampleBiomeTemperature(int worldX, int worldZ) const {
    return (biomeTemperatureNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
}

float TerrainGenerator::SampleBiomeHumidity(int worldX, int worldZ) const {
    return (biomeHumidityNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
}

float TerrainGenerator::CalculateTerrainHeightChunksForBiome(
    int worldX, int worldZ, const TerrainParams& params, BiomeType biomeType) const {

    const float terrainBaseNoise = terrainNoise->GetNoise((float)worldX, (float)worldZ);
    const float t = SampleBiomeTemperature(worldX, worldZ);
    const float h = SampleBiomeHumidity(worldX, worldZ);

    float amplitudeScale = 1.0f;
    float baseHeightOffset = 0.0f;
    float minChunks = static_cast<float>(params.MIN_TERRAIN_HEIGHT);
    float maxChunks = static_cast<float>(params.MAX_TERRAIN_HEIGHT);

    switch (biomeType) {
        case BiomeType::Plains:
            amplitudeScale = 0.36f;
            baseHeightOffset = -0.34f;
            minChunks = 4.8f;
            maxChunks = 6.4f;
            break;
        case BiomeType::FlowerForest:
            amplitudeScale = 0.60f;
            baseHeightOffset = -0.04f;
            minChunks = 4.6f;
            maxChunks = 7.4f;
            break;
        case BiomeType::Forest:
            amplitudeScale = 0.94f;
            baseHeightOffset = 0.14f;
            minChunks = 4.4f;
            maxChunks = 8.6f;
            break;
        default:
            amplitudeScale = 0.94f;
            baseHeightOffset = 0.14f;
            minChunks = 4.4f;
            maxChunks = 8.6f;
            break;
    }

    const float climateWarp = ((t - 0.5f) * 0.10f) + ((h - 0.5f) * 0.10f);
    const float biomeAmplitude = params.AMPLITUDE * amplitudeScale;
    float terrainHeightChunks = (params.BASE_HEIGHT + baseHeightOffset) + ((terrainBaseNoise + climateWarp) * biomeAmplitude);

    if (biomeType == BiomeType::Plains && terrainBaseNoise > 0.84f) {
        terrainHeightChunks += (terrainBaseNoise - 0.84f) * 1.1f;
    } else if (biomeType == BiomeType::FlowerForest && terrainBaseNoise > 0.68f) {
        terrainHeightChunks += (terrainBaseNoise - 0.68f) * 1.8f;
    } else if (biomeType == BiomeType::Forest && terrainBaseNoise > 0.56f) {
        terrainHeightChunks += (terrainBaseNoise - 0.56f) * 3.4f;
    }

    terrainHeightChunks = std::max(minChunks, std::min(maxChunks, terrainHeightChunks));
    return terrainHeightChunks;
}

int TerrainGenerator::CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params, BiomeType /*biomeType*/) const {
    const float t = SampleBiomeTemperature(worldX, worldZ);
    const float h = SampleBiomeHumidity(worldX, worldZ);
    const BiomeWeights weights = ComputeBiomeWeights(t, h);

    float terrainHeightChunks =
        (weights.plains * CalculateTerrainHeightChunksForBiome(worldX, worldZ, params, BiomeType::Plains)) +
        (weights.flowerForest * CalculateTerrainHeightChunksForBiome(worldX, worldZ, params, BiomeType::FlowerForest)) +
        (weights.forest * CalculateTerrainHeightChunksForBiome(worldX, worldZ, params, BiomeType::Forest));

    terrainHeightChunks = std::max(
        static_cast<float>(params.MIN_TERRAIN_HEIGHT),
        std::min(static_cast<float>(params.MAX_TERRAIN_HEIGHT), terrainHeightChunks));

    return static_cast<int>(terrainHeightChunks * sizes::CHUNK_SIZE);
}

uint32_t TerrainGenerator::DetermineBlockState(int worldX, int worldY, int worldZ, int depthFromSurface, 
                                              const TerrainParams& params,
                                              std::vector<glm::ivec3>& treePlacementPositions,
                                              std::vector<glm::ivec3>& flowerPlacementPositions,
                                              const blockstate::BlockStateRegistry* bsrOverride, BiomeType biomeType) {
    const blockstate::BlockStateRegistry* bsr = bsrOverride ? bsrOverride : m_bsr;
    if (!bsr) return 0u;
    uint32_t grassId = bsr->GetDefaultState("minecraft:grass");
    uint32_t dirtId = bsr->GetDefaultState("minecraft:dirt");
    uint32_t stoneId = bsr->GetDefaultState("minecraft:stone");
    const int baseDirtDepth = params.DIRT_DEPTH;
    const BiomeWeights weights = ComputeBiomeWeights(
        SampleBiomeTemperature(worldX, worldZ),
        SampleBiomeHumidity(worldX, worldZ));
    const float biomeDepthOffset =
        (weights.plains * -1.0f) +
        (weights.flowerForest * 0.0f) +
        (weights.forest * 1.0f);
    const float depthNoise = (surfaceDepthNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
    const int depthFromNoise = (depthNoise < 0.25f) ? -1 : ((depthNoise > 0.75f) ? 1 : 0);
    const int biomeDirtDepth = std::max(1, baseDirtDepth + static_cast<int>(std::round(biomeDepthOffset)) + depthFromNoise);

    if (depthFromSurface == 0) {
        if (worldY < params.SEA_LEVEL) {
            return dirtId;
        }
        CheckTreePlacement(worldX, worldY, worldZ, params, biomeType, treePlacementPositions);

        const float flowerNoise = flowerDensityNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
        const float flowerChance = (flowerNoise + 1.0f) * 0.5f;
        if (weights.flowerForest > 0.22f) {
            const float fineFlower = (flowerDensityNoise->GetNoise(
                static_cast<float>(worldX) * 0.13f, static_cast<float>(worldZ) * 0.13f) + 1.0f) * 0.5f;
            const float spreadRoll = RandomFloat(worldX + 19, worldZ - 23);
            const float targetChance = 0.50f * weights.flowerForest;
            const float dither = (fineFlower - 0.5f) * 0.22f;
            if (spreadRoll < std::clamp(targetChance + dither, 0.0f, 1.0f)) {
                flowerPlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
            }
        } else if (weights.plains > 0.25f && flowerChance > 0.90f &&
                   RandomFloat(worldX, worldZ) < 0.04f * weights.plains) {
            flowerPlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        } else if (weights.forest > 0.25f && flowerChance > 0.92f &&
                   RandomFloat(worldX + 3, worldZ - 5) < 0.02f * weights.forest) {
            flowerPlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        }

        return blockplacement::FinalizePlacedStateBasic(
            *bsr, grassId, worldX, worldY, worldZ, blockplacement::PlacementContext::TerrainGeneration
        );
    } else if (depthFromSurface < biomeDirtDepth) {
        return dirtId;
    } else {
        return stoneId;
    }
}

void TerrainGenerator::CheckTreePlacement(
    int worldX, int worldY, int worldZ,
    const TerrainParams& params,
    BiomeType biomeType,
    std::vector<glm::ivec3>& treePlacementPositions) const
{
    constexpr int MIN_TREE_SPACING = 6;

    if (worldY < params.SEA_LEVEL)
        return;

    for (const auto& existing : treePlacementPositions) {
        if (std::abs(existing.x - worldX) < MIN_TREE_SPACING &&
            std::abs(existing.z - worldZ) < MIN_TREE_SPACING) {
            return;
        }
    }

    float forestDensity = forestDensityNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    forestDensity = (forestDensity + 1.0f) * 0.5f;

    const BiomeWeights weights = ComputeBiomeWeights(
        SampleBiomeTemperature(worldX, worldZ),
        SampleBiomeHumidity(worldX, worldZ));
    const float biomeTreeDensityMul =
        (weights.plains * 0.12f) +
        (weights.flowerForest * 0.78f) +
        (weights.forest * 0.88f);
    forestDensity = std::clamp(forestDensity * biomeTreeDensityMul, 0.0f, 1.0f);

    const float minDensity = 0.05f * (biomeTreeDensityMul / 0.88f);
    if (forestDensity < minDensity) {
        return;
    }

    float noiseHash = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));

    float north = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ + 1));
    float south = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ - 1));
    float east = treeNoise->GetNoise(static_cast<float>(worldX + 1), static_cast<float>(worldZ));
    float west = treeNoise->GetNoise(static_cast<float>(worldX - 1), static_cast<float>(worldZ));

    bool isLocalMax = (noiseHash > north) && (noiseHash > south) &&
                      (noiseHash > east) && (noiseHash > west);

    if (isLocalMax) {
        float normalizedHash = (noiseHash + 1.0f) * 0.5f;
        const float densityFactor = 0.42f - weights.plains * 0.08f - weights.flowerForest * 0.10f;
        float threshold = 1.0f - (forestDensity * densityFactor);

        if (normalizedHash > threshold) {
            treePlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        }
    }
}