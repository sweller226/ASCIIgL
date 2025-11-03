#include <ASCIICraft/world/TerrainGenerator.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_set>

#include <FastNoiseLite/FastNoiseLite.h>

TerrainGenerator::TerrainGenerator() 
    : noiseInitialized(false) {
    terrainNoise = std::make_unique<FastNoiseLite>();
    caveNoise1 = std::make_unique<FastNoiseLite>();
    caveNoise2 = std::make_unique<FastNoiseLite>();
    treeNoise = std::make_unique<FastNoiseLite>();
    forestDensityNoise = std::make_unique<FastNoiseLite>();
}

TerrainGenerator::~TerrainGenerator() = default;

void TerrainGenerator::GenerateChunk(Chunk* chunk, SetBlockQuietCallback setBlockQuiet) {
    if (!chunk) return;
    
    InitializeNoiseGenerators();
    const TerrainParams params = GetTerrainParams();
    const ChunkCoord coord = chunk->GetCoord();
    
    std::vector<glm::ivec3> treePlacementPositions;
    GenerateTerrainColumn(chunk, coord, params, treePlacementPositions);
    GenerateTrees(chunk, treePlacementPositions, setBlockQuiet);
    
    chunk->SetGenerated(true);
}

void TerrainGenerator::GenerateTerrainColumn(Chunk* chunk, const ChunkCoord& coord, const TerrainParams& params,
                                            std::vector<glm::ivec3>& treePlacementPositions) {
    const int chunkBaseY = coord.y * Chunk::SIZE;
    
    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int z = 0; z < Chunk::SIZE; ++z) {
            const glm::ivec3 worldPos = LocalToWorldPos(coord, x, z);
            const int terrainHeight = CalculateTerrainHeight(worldPos.x, worldPos.z, params);
            
            for (int y = 0; y < Chunk::SIZE; ++y) {
                const int worldY = chunkBaseY + y;
                const BlockType blockType = GetBlockTypeAt(worldPos.x, worldY, worldPos.z, terrainHeight, params, treePlacementPositions);
                
                if (blockType != BlockType::Air) {
                    chunk->SetBlock(x, y, z, Block(blockType));
                }
            }
        }
    }
}

void TerrainGenerator::GenerateTrees(Chunk* chunk, const std::vector<glm::ivec3>& treePlacementPositions,
                                    SetBlockQuietCallback setBlockQuiet) {
    if (!setBlockQuiet || treePlacementPositions.empty()) return;
    
    std::unordered_set<Chunk*> affectedChunks;
    affectedChunks.insert(chunk);
    
    for (const auto& pos : treePlacementPositions) {
        GenerateTree(pos.x, pos.y, pos.z, setBlockQuiet, affectedChunks);
    }
    
    for (auto* affectedChunk : affectedChunks) {
        affectedChunk->InvalidateMesh();
    }
}

glm::ivec3 TerrainGenerator::LocalToWorldPos(const ChunkCoord& coord, int localX, int localZ) const {
    return glm::ivec3(
        coord.x * Chunk::SIZE + localX,
        0, // Y will be calculated per-block
        coord.z * Chunk::SIZE + localZ
    );
}

BlockType TerrainGenerator::GetBlockTypeAt(int worldX, int worldY, int worldZ, int terrainHeight,
                                          const TerrainParams& params, std::vector<glm::ivec3>& treePlacementPositions) {
    // Bedrock layer at world bottom
    if (worldY == 0) {
        return BlockType::Bedrock;
    }
    
    // Air above terrain
    if (worldY > terrainHeight) {
        return BlockType::Air;
    }
    
    // Check for cave carving
    const int depthFromSurface = terrainHeight - worldY;
    if (ShouldCarveCave(worldX, worldY, worldZ, depthFromSurface, params)) {
        return BlockType::Air;
    }
    
    // Determine solid block type
    return DetermineBlockType(worldX, worldY, worldZ, depthFromSurface, params, treePlacementPositions);
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
    caveNoise1->SetFractalOctaves(5);  // Fewer octaves = smoother, longer curves
    caveNoise1->SetFrequency(0.008f);   // MUCH lower = longer winding tunnels
    caveNoise1->SetFractalLacunarity(2.0f);
    caveNoise1->SetFractalGain(0.5f);
    caveNoise1->SetSeed(54321);

    // Second cave noise layer - adds variety and connections
    caveNoise2->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    caveNoise2->SetFractalType(FastNoiseLite::FractalType_FBm);
    caveNoise2->SetFractalOctaves(5);
    caveNoise2->SetFrequency(0.010f);   // Slightly different for variation
    caveNoise2->SetFractalLacunarity(2.0f);
    caveNoise2->SetFractalGain(0.5f);
    caveNoise2->SetSeed(98765);

    // Forest density noise — big forest regions
    forestDensityNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    forestDensityNoise->SetFrequency(0.01f);  // lower = bigger biomes
    forestDensityNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
    forestDensityNoise->SetFractalOctaves(4);
    forestDensityNoise->SetSeed(11111);

    // Tree patch noise — small-scale patches/clumps
    treeNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    treeNoise->SetFrequency(0.25f);  // smaller patches (~4 blocks wide)
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
    params.CAVE_THRESHOLD = 0.25f;  // Adjust for cave density (0.2-0.4)
    params.VERTICAL_STRETCH = 0.2f;  // Higher = more horizontal (try 3.5-5.0)
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
    
    // Perlin Worm cave generation - stretch Y for horizontal tunnels
    const float stretchedY = static_cast<float>(worldY) * params.VERTICAL_STRETCH;
    
    // Sample two noise fields for dual tunnel networks
    const float cave1 = caveNoise1->GetNoise(static_cast<float>(worldX), stretchedY, static_cast<float>(worldZ));
    const float cave2 = caveNoise2->GetNoise(static_cast<float>(worldX), stretchedY, static_cast<float>(worldZ));
    
    // Calculate surface fade-in threshold (prevents caves breaking through surface)
    const float surfaceFade = (depthFromSurface < 8) 
        ? params.CAVE_THRESHOLD + (1.0f - depthFromSurface / 8.0f) * 0.3f
        : params.CAVE_THRESHOLD;
    
    // Cave exists if either tunnel exceeds the threshold
    return (cave1 > surfaceFade) || (cave2 > surfaceFade);
}

BlockType TerrainGenerator::DetermineBlockType(int worldX, int worldY, int worldZ, int depthFromSurface, 
                                              const TerrainParams& params,
                                              std::vector<glm::ivec3>& treePlacementPositions) {
    if (depthFromSurface == 0) {
        // Surface block - grass
        CheckTreePlacement(worldX, worldY, worldZ, params, treePlacementPositions);
        return BlockType::Grass;
    } else if (depthFromSurface < params.DIRT_DEPTH) {
        // Shallow subsurface - dirt
        return BlockType::Dirt;
    } else {
        // Deep underground - stone
        return BlockType::Stone;
    }
}

void TerrainGenerator::CheckTreePlacement(
    int worldX, int worldY, int worldZ,
    const TerrainParams& params,
    std::vector<glm::ivec3>& treePlacementPositions) const
{
    // Minecraft's actual approach: Chunk-based scatter with random attempts
    // Trees are NOT placed per-block, but via random scatter attempts per chunk
    // This naturally prevents clustering while allowing organic distribution
    
    // Layer 1: Forest density (determines attempts per chunk)
    float forestDensity = forestDensityNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    forestDensity = (forestDensity + 1.0f) * 0.5f; // [0, 1]
    
    if (forestDensity < 0.2f) {
        return; // Non-forest biome
    }
    
    // Minecraft's trick: Use noise as a hash to create scatter points
    // Only certain "special" positions can have trees (like Poisson disk sampling)
    float noiseHash = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    
    // Trees can only spawn where noise creates a "peak" (local maximum)
    // Check if this position is higher than all 4 neighbors
    float north = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ + 1));
    float south = treeNoise->GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ - 1));
    float east = treeNoise->GetNoise(static_cast<float>(worldX + 1), static_cast<float>(worldZ));
    float west = treeNoise->GetNoise(static_cast<float>(worldX - 1), static_cast<float>(worldZ));
    
    // This position is a "candidate" if it's a local maximum
    bool isLocalMax = (noiseHash > north) && (noiseHash > south) && 
                      (noiseHash > east) && (noiseHash > west);
    
    if (isLocalMax) {
        // Probabilistic spawn based on forest density
        float normalizedHash = (noiseHash + 1.0f) * 0.5f;
        float threshold = 1.0f - (forestDensity * 0.5f); // Dense forest = lower threshold
        
        if (normalizedHash > threshold) {
            treePlacementPositions.push_back(glm::ivec3(worldX, worldY + 1, worldZ));
        }
    }
}

void TerrainGenerator::GenerateGrassLayerChunk(Chunk* chunk) {
    if (!chunk) return;
    
    // Generate a simple test pattern
    if (chunk->GetCoord().y == 0) {
        // Generate grass layer at y=0 chunks
        for (int x = 0; x < Chunk::SIZE; ++x) {
            for (int z = 0; z < Chunk::SIZE; ++z) {
                chunk->SetBlock(x, 0, z, Block(BlockType::Grass));
            }
        }
    }
    
    // Mark chunk as generated and dirty (mesh will be generated in RegenerateDirtyChunks)
    chunk->SetGenerated(true);
    chunk->SetDirty(true);
}

void TerrainGenerator::GenerateRandomBlockChunk(Chunk* chunk) {
    if (!chunk) return;
    
    ChunkCoord coord = chunk->GetCoord();

    // Define available block types (excluding Air and Bedrock for now)
    static const std::vector<BlockType> blockTypes = {
        BlockType::Stone,
        BlockType::Dirt,
        BlockType::Grass,
        BlockType::Wood,
        BlockType::Leaves,
        BlockType::Gravel,
        BlockType::Coal_Ore,
        BlockType::Iron_Ore,
        BlockType::Diamond_Ore,
        BlockType::Cobblestone,
        BlockType::Crafting_Table,
        BlockType::Wood_Planks,
        BlockType::Furnace,
        BlockType::Bedrock,
    };
    
    // Use chunk coordinates as seed for consistent generation
    std::srand(coord.x * 1000 + coord.y * 100 + coord.z * 10);
    
    if (coord.y == 0) {
        // Generate random blocks at y=0 chunks with some structure
        for (int x = 0; x < Chunk::SIZE; ++x) {
            for (int z = 0; z < Chunk::SIZE; ++z) {
                // Create some variation: 80% chance of blocks, 20% air
                if (std::rand() % 100 < 80) {
                    // Pick a random block type
                    BlockType randomBlock = blockTypes[std::rand() % blockTypes.size()];
                    chunk->SetBlock(x, 0, z, Block(randomBlock));
                    
                    // Sometimes add a second layer for variety
                    if (std::rand() % 100 < 30) {
                        BlockType secondBlock = blockTypes[std::rand() % blockTypes.size()];
                        chunk->SetBlock(x, 1, z, Block(secondBlock));
                    }
                }
                // 20% chance remains Air (empty space)
            }
        }
    } else if (coord.y > 0) {
        // Upper chunks: more sparse, mostly air with occasional blocks
        for (int x = 0; x < Chunk::SIZE; ++x) {
            for (int z = 0; z < Chunk::SIZE; ++z) {
                for (int y = 0; y < Chunk::SIZE; ++y) {
                    // Only 10% chance of blocks in upper chunks
                    if (std::rand() % 100 < 10) {
                        BlockType randomBlock = blockTypes[std::rand() % blockTypes.size()];
                        chunk->SetBlock(x, y, z, Block(randomBlock));
                    }
                }
            }
        }
    }
    
    // Mark chunk as generated and dirty (mesh will be generated in RegenerateDirtyChunks)
    chunk->SetGenerated(true);
    chunk->SetDirty(true);
}

void TerrainGenerator::GenerateOneBlockGrassChunk(Chunk* chunk) {
    if (!chunk) return;

    ChunkCoord coord = chunk->GetCoord();
    
    chunk->SetBlock(0, 0, 0, Block(BlockType::Grass));
    chunk->SetGenerated(true);
    chunk->SetDirty(true);
}

void TerrainGenerator::GenerateTree(int worldX, int worldY, int worldZ,
                                   SetBlockQuietCallback setBlockQuiet, std::unordered_set<Chunk*>& affectedChunks) {
    // Tree structure constants
    constexpr int TRUNK_HEIGHT = 5;
    constexpr int LEAF_BASE_OFFSET = 3;
    constexpr int LEAF_RADIUS = 2;
    
    // Generate trunk
    setBlockQuiet(worldX, worldY - 1, worldZ, Block(BlockType::Dirt), affectedChunks);
    for (int i = 0; i < TRUNK_HEIGHT; ++i) {
        setBlockQuiet(worldX, worldY + i, worldZ, Block(BlockType::Wood), affectedChunks);
    }
    
    const int leafBaseY = worldY + LEAF_BASE_OFFSET;
    
    // Bottom two layers (Y+3, Y+4): 5x5 without corners
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = -LEAF_RADIUS; dx <= LEAF_RADIUS; ++dx) {
            for (int dz = -LEAF_RADIUS; dz <= LEAF_RADIUS; ++dz) {
                if (std::abs(dx) == LEAF_RADIUS && std::abs(dz) == LEAF_RADIUS) continue; // Skip corners
                setBlockQuiet(worldX + dx, leafBaseY + dy, worldZ + dz, Block(BlockType::Leaves), affectedChunks);
            }
        }
    }
    
    // Third layer (Y+5): 3x3
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            setBlockQuiet(worldX + dx, leafBaseY + 2, worldZ + dz, Block(BlockType::Leaves), affectedChunks);
        }
    }
    
    // Top layer (Y+6): Crown
    const int crownY = leafBaseY + 3;
    setBlockQuiet(worldX, crownY, worldZ + 1, Block(BlockType::Leaves), affectedChunks);
    setBlockQuiet(worldX, crownY, worldZ - 1, Block(BlockType::Leaves), affectedChunks);
    setBlockQuiet(worldX + 1, crownY, worldZ, Block(BlockType::Leaves), affectedChunks);
    setBlockQuiet(worldX - 1, crownY, worldZ, Block(BlockType::Leaves), affectedChunks);
}