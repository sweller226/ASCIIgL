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
}

TerrainGenerator::~TerrainGenerator() = default;

void TerrainGenerator::GenerateChunk(Chunk* chunk,
                                    SetBlockQuietCallback setBlockQuiet,
                                    BatchInvalidateCallback batchInvalidate) {
    if (!chunk) return;
    
    ChunkCoord coord = chunk->GetCoord();
    
    InitializeNoiseGenerators();
    
    const TerrainParams params = GetTerrainParams();
    int chunkBaseY = coord.y * Chunk::SIZE;
    std::vector<std::tuple<int, int, int>> treePlacementPositions;
    
    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int z = 0; z < Chunk::SIZE; ++z) {
            int worldX = coord.x * Chunk::SIZE + x;
            int worldZ = coord.z * Chunk::SIZE + z;
            int terrainHeight = CalculateTerrainHeight(worldX, worldZ, params);
            
            GenerateTerrainColumn(chunk, x, z, worldX, worldZ, chunkBaseY, terrainHeight, params, treePlacementPositions);
        }
    }

    // Generate trees if callbacks are provided
    if (setBlockQuiet && batchInvalidate) {
        for (const auto& pos : treePlacementPositions) {
            GenerateTree(std::get<0>(pos), std::get<1>(pos), std::get<2>(pos), setBlockQuiet, batchInvalidate);
        }
    }
    
    chunk->SetGenerated(true);
    chunk->SetDirty(true);
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

    // Tree placement noise
    treeNoise->SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    treeNoise->SetFrequency(0.05f);
    treeNoise->SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    treeNoise->SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);
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
    params.TREE_CHANCE_THRESHOLD = 0.35f;
    return params;
}

int TerrainGenerator::CalculateTerrainHeight(int worldX, int worldZ, const TerrainParams& params) const {
    float noiseValue = terrainNoise->GetNoise((float)worldX, (float)worldZ);
    float terrainHeightChunks = params.BASE_HEIGHT + (noiseValue * params.AMPLITUDE);
    terrainHeightChunks = std::max((float)params.MIN_TERRAIN_HEIGHT, std::min((float)params.MAX_TERRAIN_HEIGHT, terrainHeightChunks));
    return (int)(terrainHeightChunks * Chunk::SIZE);
}

void TerrainGenerator::GenerateTerrainColumn(Chunk* chunk, int x, int z, int worldX, int worldZ, int chunkBaseY, 
                                            int terrainHeight, const TerrainParams& params,
                                            std::vector<std::tuple<int, int, int>>& treePlacementPositions) {
    for (int y = 0; y < Chunk::SIZE; ++y) {
        int worldY = chunkBaseY + y;
        
        // Bedrock at Y=0
        if (worldY == 0) {
            chunk->SetBlock(x, y, z, Block(BlockType::Bedrock));
            continue;
        }
        
        // Skip blocks above terrain surface
        if (worldY > terrainHeight) continue;
        
        int depthFromSurface = terrainHeight - worldY;
        
        // Check if this block should be carved out as a cave
        if (ShouldCarveCave(worldX, worldY, worldZ, depthFromSurface, params)) {
            continue;  // Leave as air
        }
        
        // Determine what block type to place
        BlockType blockType = DetermineBlockType(worldX, worldY, worldZ, depthFromSurface, params, treePlacementPositions);
        chunk->SetBlock(x, y, z, Block(blockType));
    }
}

bool TerrainGenerator::ShouldCarveCave(int worldX, int worldY, int worldZ, int depthFromSurface, const TerrainParams& params) const {
    // Only carve caves in the specified height range
    if (worldY < params.MIN_CAVE_HEIGHT) {
        return false;
    }
    
    // Perlin Worm cave generation - TRUE Minecraft-style approach
    // Stretch Y coordinate to bias horizontal tunnels (like Minecraft's coordinate scaling)
    float stretchedY = (float)worldY * params.VERTICAL_STRETCH;
    
    // Sample two noise fields at different scales
    float cave1 = caveNoise1->GetNoise((float)worldX, stretchedY, (float)worldZ);
    float cave2 = caveNoise2->GetNoise((float)worldX, stretchedY, (float)worldZ);
    
    // Caves form where EITHER noise field is above threshold
    // This creates TWO separate tunnel networks that occasionally intersect
    // Using OR instead of AND gives proper worm-like tunnels
    bool isCave1 = cave1 > params.CAVE_THRESHOLD;
    bool isCave2 = cave2 > params.CAVE_THRESHOLD;
    
    if (!isCave1 && !isCave2) {
        return false; // Neither tunnel system present
    }
    
    // Prevent caves from breaking through near the surface for more organic transitions
    // The closer to the surface, the HIGHER the threshold (harder to form caves)
    if (depthFromSurface < 8) {
        float surfaceFade = (float)depthFromSurface / 8.0f; // 0.0 at surface, 1.0 at depth 8
        // As we get closer to surface (surfaceFade -> 0), threshold increases dramatically
        float adjustedThreshold = params.CAVE_THRESHOLD + (1.0f - surfaceFade) * 0.3f; // Add up to 0.3
        
        // Check if either cave passes the adjusted threshold
        bool cave1Passes = isCave1 && cave1 > adjustedThreshold;
        bool cave2Passes = isCave2 && cave2 > adjustedThreshold;
        
        if (!cave1Passes || !cave2Passes) {
            return false; // Both tunnels filtered by surface protection
        }
    }
    
    return true;
}

BlockType TerrainGenerator::DetermineBlockType(int worldX, int worldY, int worldZ, int depthFromSurface, 
                                              const TerrainParams& params,
                                              std::vector<std::tuple<int, int, int>>& treePlacementPositions) {
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

void TerrainGenerator::CheckTreePlacement(int worldX, int worldY, int worldZ, const TerrainParams& params,
                                         std::vector<std::tuple<int, int, int>>& treePlacementPositions) const {
    // Only check tree placement on a grid (every 8 blocks)
    if ((worldX % 8 != 0) || (worldZ % 8 != 0)) return;
    
    float treeValue = (treeNoise->GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
    
    if (treeValue < params.TREE_CHANCE_THRESHOLD) {
        treePlacementPositions.push_back({worldX, worldY + 1, worldZ});
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
                                   SetBlockQuietCallback setBlockQuiet,
                                   BatchInvalidateCallback batchInvalidate) {
    // Classic Minecraft oak tree: 4-6 blocks tall trunk + leaf crown
    const int TRUNK_HEIGHT = 5;
    
    // Track which chunks we modify so we can invalidate them once at the end
    std::unordered_set<ChunkCoord> affectedChunks;
    
    // Generate trunk (wood blocks)
    setBlockQuiet(worldX, worldY - 1, worldZ, Block(BlockType::Dirt), affectedChunks); // Ensure dirt below trunk
    for (int i = 0; i < TRUNK_HEIGHT; ++i) {
        setBlockQuiet(worldX, worldY + i, worldZ, Block(BlockType::Wood), affectedChunks);
    }
    
    // Leaf crown - Minecraft style from bottom to top:
    // Layer 0 (Y+3): 5x5 base (no corners)
    // Layer 1 (Y+4): 5x5 full square
    // Layer 2 (Y+5): 3x3 around top of trunk
    // Layer 3 (Y+6): Single leaf block on top
    
    int leafBaseY = worldY + 3; // Start leaves 3 blocks up
    
    // Bottom leaf layer (Y+3): 5x5 without corners
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            // Skip corners
            if (std::abs(dx) == 2 && std::abs(dz) == 2) continue;
            
            setBlockQuiet(worldX + dx, leafBaseY, worldZ + dz, Block(BlockType::Leaves), affectedChunks);
        }
    }
    
    // Second leaf layer (Y+4): Full 5x5
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            setBlockQuiet(worldX + dx, leafBaseY + 1, worldZ + dz, Block(BlockType::Leaves), affectedChunks);
        }
    }
    
    // Third leaf layer (Y+5): 3x3
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            setBlockQuiet(worldX + dx, leafBaseY + 2, worldZ + dz, Block(BlockType::Leaves), affectedChunks);
        }
    }
    
    // Top leaf (Y+6): Single block
    setBlockQuiet(worldX, leafBaseY + 3, worldZ, Block(BlockType::Leaves), affectedChunks);
}