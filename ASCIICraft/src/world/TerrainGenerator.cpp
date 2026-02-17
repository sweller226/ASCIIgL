#include <ASCIICraft/world/TerrainGenerator.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_set>

#include <FastNoiseLite/FastNoiseLite.h>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIICraft/world/blockplacement/BlockPlacement.hpp>

TerrainGenerator::TerrainGenerator(entt::registry &registry)
    : noiseInitialized(false)
    , m_registry(registry) {
    terrainNoise = std::make_unique<FastNoiseLite>();
    caveNoise1 = std::make_unique<FastNoiseLite>();
    caveNoise2 = std::make_unique<FastNoiseLite>();
    treeNoise = std::make_unique<FastNoiseLite>();
    forestDensityNoise = std::make_unique<FastNoiseLite>();

    if (registry.ctx().contains<blockstate::BlockStateRegistry>()) {
        m_bsr = &registry.ctx().get<blockstate::BlockStateRegistry>();
    } else {
        ASCIIgL::Logger::Warning("BlockStateRegistry missing from context");
        m_bsr = nullptr;
    }
}   

TerrainGenerator::~TerrainGenerator() = default;

void TerrainGenerator::GenerateChunk(Chunk* chunk, SetBlockCallback setBlock) {
    if (!chunk) return;
    
    InitializeNoiseGenerators();
    const TerrainParams params = GetTerrainParams();
    const ChunkCoord coord = chunk->GetCoord();
    
    std::vector<glm::ivec3> treePlacementPositions;
    GenerateTerrainColumns(chunk, coord, params, treePlacementPositions);
    GenerateTrees(chunk, treePlacementPositions, setBlock);
    
    chunk->SetGenerated(true);
}

void TerrainGenerator::GenerateTerrainColumns(Chunk* chunk, const ChunkCoord& coord, const TerrainParams& params,
                                            std::vector<glm::ivec3>& treePlacementPositions) {
    const int chunkBaseY = coord.y * Chunk::SIZE;

    
    uint32_t airId = m_bsr->GetDefaultState("minecraft:air");
    
    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int z = 0; z < Chunk::SIZE; ++z) {
            const glm::ivec3 WorldCoord = LocalToWorldCoord(coord, x, z);
            const int terrainHeight = CalculateTerrainHeight(WorldCoord.x, WorldCoord.z, params);
            
            for (int y = 0; y < Chunk::SIZE; ++y) {
                const int worldY = chunkBaseY + y;
                const uint32_t stateId = GetBlockStateAt(WorldCoord.x, worldY, WorldCoord.z, terrainHeight, params, treePlacementPositions);
                
                if (stateId != airId) {
                    chunk->SetBlockState(x, y, z, stateId);
                }
            }
        }
    }
}

void TerrainGenerator::GenerateTrees(Chunk* chunk, const std::vector<glm::ivec3>& treePlacementPositions, SetBlockCallback setBlock) {
    if (!setBlock || treePlacementPositions.empty()) return;
    
    for (const auto& pos : treePlacementPositions) {
        GenerateTree(pos.x, pos.y, pos.z, setBlock);
    }
}

glm::ivec3 TerrainGenerator::LocalToWorldCoord(const ChunkCoord& coord, int localX, int localZ) const {
    return glm::ivec3(
        coord.x * Chunk::SIZE + localX,
        0, // Y will be calculated per-block
        coord.z * Chunk::SIZE + localZ
    );
}

uint32_t TerrainGenerator::GetBlockStateAt(int worldX, int worldY, int worldZ, int terrainHeight,
                                          const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions) {

    uint32_t bedrockId = m_bsr->GetDefaultState("minecraft:bedrock");
    uint32_t airId = m_bsr->GetDefaultState("minecraft:air");

    // Bedrock layer at world bottom
    if (worldY == 0) {
        return bedrockId;
    }
    
    // Air above terrain
    if (worldY > terrainHeight) {
        return airId;
    }
    
    // Check for cave carving
    const int depthFromSurface = terrainHeight - worldY;
    if (ShouldCarveCave(worldX, worldY, worldZ, depthFromSurface, params)) {
        return airId;
    }
    
    // Determine solid block type
    return DetermineBlockState(worldX, worldY, worldZ, depthFromSurface, params, treePlacementPositions);
}

void TerrainGenerator::InitializeNoiseGenerators() {
    if (noiseInitialized) return;
    
    // Terrain noise - for surface height
    terrainNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    terrainNoise->SetFrequency(0.02f);
    terrainNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    terrainNoise->SetFractalOctaves(3);
    terrainNoise->SetFractalLacunarity(2.0f);
    terrainNoise->SetFractalGain(0.5f);
    terrainNoise->SetSeed(12345);
    
    // First cave noise layer - main worm paths (Perlin Worm method)
    caveNoise1->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    caveNoise1->SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise1->SetFractalOctaves(5);
    caveNoise1->SetFrequency(0.008f);
    caveNoise1->SetFractalLacunarity(2.0f);
    caveNoise1->SetFractalGain(0.5f);
    caveNoise1->SetSeed(54321);

    // Second cave noise layer - adds variety and connections
    caveNoise2->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    caveNoise2->SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise2->SetFractalOctaves(5);
    caveNoise2->SetFrequency(0.010f);
    caveNoise2->SetFractalLacunarity(2.0f);
    caveNoise2->SetFractalGain(0.5f);
    caveNoise2->SetSeed(98765);

    // Forest density noise
    forestDensityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    forestDensityNoise->SetFrequency(0.01f);
    forestDensityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    forestDensityNoise->SetFractalOctaves(4);
    forestDensityNoise->SetSeed(11111);

    // Tree patch noise
    treeNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    treeNoise->SetFrequency(0.25f);
    treeNoise->SetFractalType(FastNoiseLite::FractalType_None);
    treeNoise->SetSeed(99999);
    
    noiseInitialized = true;
}

TerrainGenerator::TerrainParams TerrainGenerator::GetTerrainParams() const {
    TerrainParams params;
    params.BASE_HEIGHT = 5;
    params.MIN_TERRAIN_HEIGHT = 3;
    params.MAX_TERRAIN_HEIGHT = 7;
    params.AMPLITUDE = 1.5f;
    params.DIRT_DEPTH = 3;
    params.MIN_CAVE_HEIGHT = 2;
    params.CAVE_THRESHOLD = 0.25f;
    params.VERTICAL_STRETCH = 0.2f;
    return params;
}

int TerrainGenerator::CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params) const {
    float noiseValue = terrainNoise->GetNoise((float)worldX, (float)worldZ);
    float terrainHeightChunks = params.BASE_HEIGHT + (noiseValue * params.AMPLITUDE);
    terrainHeightChunks = std::max((float)params.MIN_TERRAIN_HEIGHT, std::min((float)params.MAX_TERRAIN_HEIGHT, terrainHeightChunks));
    return (int)(terrainHeightChunks * Chunk::SIZE);
}

bool TerrainGenerator::ShouldCarveCave(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params) const {
    if (worldY < params.MIN_CAVE_HEIGHT) return false;
    
    const float stretchedY = static_cast<float>(worldY) * params.VERTICAL_STRETCH;
    
    const float cave1 = caveNoise1->GetNoise(static_cast<float>(worldX), stretchedY, static_cast<float>(worldZ));
    const float cave2 = caveNoise2->GetNoise(static_cast<float>(worldX), stretchedY, static_cast<float>(worldZ));
    
    const float surfaceFade = (depthFromSurface < 8) 
        ? params.CAVE_THRESHOLD + (1.0f - depthFromSurface / 8.0f) * 0.3f
        : params.CAVE_THRESHOLD;
    
    return (cave1 > surfaceFade) || (cave2 > surfaceFade);
}

uint32_t TerrainGenerator::DetermineBlockState(int worldX, int worldY, int worldZ, int depthFromSurface, 
                                              const TerrainParams& params,
                                              std::vector<glm::ivec3>& treePlacementPositions) {

    uint32_t grassId = m_bsr->GetDefaultState("minecraft:grass");
    uint32_t dirtId = m_bsr->GetDefaultState("minecraft:dirt");
    uint32_t stoneId = m_bsr->GetDefaultState("minecraft:stone");

    if (depthFromSurface == 0) {
        CheckTreePlacement(worldX, worldY, worldZ, params, treePlacementPositions);

        // Apply placement-time logic (grass orientation, etc.) with terrain generation context
        return ASCIICraft::GetFinalizedBlockStateForPlacement(
            *m_bsr, grassId, worldX, worldY, worldZ, ASCIICraft::PlacementContext::TerrainGeneration
        );
    } else if (depthFromSurface < params.DIRT_DEPTH) {
        return dirtId;
    } else {
        return stoneId;
    }
}

void TerrainGenerator::CheckTreePlacement(
    int worldX, int worldY, int worldZ,
    const TerrainParams& params,
    std::vector<glm::ivec3>& treePlacementPositions) const
{
    float forestDensity = forestDensityNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    forestDensity = (forestDensity + 1.0f) * 0.5f;
    
    if (forestDensity < 0.2f) {
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
        float threshold = 1.0f - (forestDensity * 0.5f);
        
        if (normalizedHash > threshold) {
            treePlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        }
    }
}

void TerrainGenerator::GenerateTree(int worldX, int worldY, int worldZ, SetBlockCallback setBlock) {
    constexpr int TRUNK_HEIGHT = 6;  // Extends up to Y+5 (center of 3x3 layer, almost to top)
    constexpr int LEAF_BASE_OFFSET = 3;
    constexpr int LEAF_RADIUS = 2;

    uint32_t dirtId = m_bsr->GetDefaultState("minecraft:dirt");
    uint32_t oakLogId = m_bsr->GetDefaultState("minecraft:oak_log");
    uint32_t oakLeavesId = m_bsr->GetDefaultState("minecraft:oak_leaves");
    
    // Generate trunk
    setBlock(worldX, worldY - 1, worldZ, dirtId);
    for (int i = 0; i < TRUNK_HEIGHT; ++i) {
        setBlock(worldX, worldY + i, worldZ, oakLogId);
    }
    
    const int leafBaseY = worldY + LEAF_BASE_OFFSET;
    
    // Bottom two layers (Y+3, Y+4): 5x5 without corners
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = -LEAF_RADIUS; dx <= LEAF_RADIUS; ++dx) {
            for (int dz = -LEAF_RADIUS; dz <= LEAF_RADIUS; ++dz) {
                if (std::abs(dx) == LEAF_RADIUS && std::abs(dz) == LEAF_RADIUS) continue;
                setBlock(worldX + dx, leafBaseY + dy, worldZ + dz, oakLeavesId);
            }
        }
    }
    
    // Third layer (Y+5): 3x3
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            setBlock(worldX + dx, leafBaseY + 2, worldZ + dz, oakLeavesId);
        }
    }
    
    // Top layer (Y+6): Crown
    const int crownY = leafBaseY + 3;
    setBlock(worldX, crownY, worldZ, oakLeavesId);
    setBlock(worldX, crownY, worldZ + 1, oakLeavesId);
    setBlock(worldX, crownY, worldZ - 1, oakLeavesId);
    setBlock(worldX + 1, crownY, worldZ, oakLeavesId);
    setBlock(worldX - 1, crownY, worldZ, oakLeavesId);
}