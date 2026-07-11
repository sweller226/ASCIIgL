#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>

#include "TerrainGenerator_Internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

using namespace terrain_generator_internal;

namespace {

struct TreeNode {
    int x;
    int y;
    int z;
    int radius;
    int foliageHeight;
};

struct TreeConfig {
    int minHeight;
    int maxHeight;
    int minBranches;
    int maxBranches;
    int minBranchLength;
    int maxBranchLength;
    int canopyRadius;
    int canopyHeight;
};

struct TreeBuildContext {
    const blockstate::BlockStateRegistry& bsr;
    std::vector<WorldBlockPlacement>& out;
    uint32_t dirtId;
    uint32_t logId;
    uint32_t logXId;
    uint32_t logZId;
    uint32_t leavesId;
    std::unordered_set<uint64_t> logPositions;
    std::unordered_set<uint64_t> leafPositions;
};

uint64_t BlockKey(int x, int y, int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42) ^
           (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 21) ^
           static_cast<uint64_t>(static_cast<uint32_t>(z));
}

uint32_t AxisLogId(const TreeBuildContext& ctx, int dx, int dz) {
    if (std::abs(dx) >= std::abs(dz) && dx != 0) {
        return ctx.logXId;
    }
    if (dz != 0) {
        return ctx.logZId;
    }
    return ctx.logId;
}

void PushBlock(std::vector<WorldBlockPlacement>& out, int x, int y, int z, uint32_t stateId) {
    out.push_back(WorldBlockPlacement{ WorldCoord(x, y, z), stateId });
}

void PushDirt(TreeBuildContext& ctx, int x, int y, int z) {
    PushBlock(ctx.out, x, y, z, ctx.dirtId);
}

void PushLog(TreeBuildContext& ctx, int x, int y, int z, uint32_t stateId) {
    ctx.logPositions.insert(BlockKey(x, y, z));
    PushBlock(ctx.out, x, y, z, stateId);
}

void PushLeaf(TreeBuildContext& ctx, int x, int y, int z) {
    const uint64_t key = BlockKey(x, y, z);
    if (ctx.logPositions.count(key) || !ctx.leafPositions.insert(key).second) {
        return;
    }
    PushBlock(ctx.out, x, y, z, ctx.leavesId);
}

TreeBuildContext MakeTreeContext(const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    const uint32_t logId = bsr->GetDefaultState("minecraft:oak_log");
    return TreeBuildContext{
        *bsr,
        out,
        bsr->GetDefaultState("minecraft:dirt"),
        logId,
        bsr->WithProperty(logId, "axis", "x"),
        bsr->WithProperty(logId, "axis", "z"),
        bsr->GetDefaultState("minecraft:oak_leaves"),
        {},
        {}
    };
}

int ConfigHeight(const TreeConfig& config, int worldX, int worldZ) {
    return RandomRange(worldX + config.minHeight * 19, worldZ - config.maxHeight * 23, config.minHeight, config.maxHeight);
}

void PlaceStraightTrunk(TreeBuildContext& ctx, int worldX, int worldY, int worldZ, int trunkHeight) {
    PushDirt(ctx, worldX, worldY - 1, worldZ);
    for (int i = 0; i < trunkHeight; ++i) {
        PushLog(ctx, worldX, worldY + i, worldZ, ctx.logId);
    }
}

void PlaceGiantTrunk(TreeBuildContext& ctx, int worldX, int worldY, int worldZ, int trunkHeight) {
    for (int ox = 0; ox <= 1; ++ox) {
        for (int oz = 0; oz <= 1; ++oz) {
            PushDirt(ctx, worldX + ox, worldY - 1, worldZ + oz);
            for (int i = 0; i < trunkHeight; ++i) {
                PushLog(ctx, worldX + ox, worldY + i, worldZ + oz, ctx.logId);
            }
        }
    }
}

void PlaceBranchLine(TreeBuildContext& ctx, int startX, int startY, int startZ, int endX, int endY, int endZ) {
    const int dx = endX - startX;
    const int dy = endY - startY;
    const int dz = endZ - startZ;
    const int steps = std::max({std::abs(dx), std::abs(dy), std::abs(dz), 1});
    const uint32_t axisId = AxisLogId(ctx, dx, dz);

    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = startX + static_cast<int>(std::round(static_cast<float>(dx) * t));
        const int y = startY + static_cast<int>(std::round(static_cast<float>(dy) * t));
        const int z = startZ + static_cast<int>(std::round(static_cast<float>(dz) * t));
        PushLog(ctx, x, y, z, axisId);
    }
}

bool ShouldSkipLeafCorner(int cx, int cy, int cz, int dx, int dy, int dz, int radius) {
    if (radius <= 1) return false;
    const bool hardCorner = std::abs(dx) == radius && std::abs(dz) == radius;
    if (!hardCorner) return false;

    const float roll = RandomFloat(cx + dx * 31 + dy * 17, cz + dz * 37 - radius * 13);
    return roll > 0.18f;
}

void PlaceFoliageLayer(TreeBuildContext& ctx, int cx, int y, int cz, int radius, bool protectCenter) {
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            if (protectCenter && dx == 0 && dz == 0) continue;
            if (ShouldSkipLeafCorner(cx, y, cz, dx, 0, dz, radius)) continue;

            const int manhattanEdge = std::abs(dx) + std::abs(dz);
            if (radius > 1 && manhattanEdge > radius + 1) continue;
            PushLeaf(ctx, cx + dx, y, cz + dz);
        }
    }
}

void PlaceBlobFoliage(TreeBuildContext& ctx, const TreeNode& node, bool protectCenter) {
    for (int layer = 0; layer < node.foliageHeight; ++layer) {
        int radius = node.radius;
        if (layer == 0 && node.foliageHeight > 2) {
            radius = std::max(1, node.radius - 1);
        } else if (layer == node.foliageHeight - 1) {
            radius = std::max(1, node.radius - 1);
        }
        const bool protectLayerCenter = protectCenter && layer < node.foliageHeight - 1;
        PlaceFoliageLayer(ctx, node.x, node.y + layer, node.z, radius, protectLayerCenter);
    }
}

void PlaceFancyFoliage(TreeBuildContext& ctx, const std::vector<TreeNode>& nodes, bool protectCenter) {
    for (const TreeNode& node : nodes) {
        PlaceBlobFoliage(ctx, node, protectCenter);
    }
}

std::vector<TreeNode> GenerateFoliageNodes(int worldX, int worldY, int worldZ, int trunkHeight,
                                           const TreeConfig& config, bool giantTrunk) {
    std::vector<TreeNode> nodes;

    const int branchCount = RandomRange(
        worldX + trunkHeight * 11,
        worldZ - trunkHeight * 13,
        config.minBranches,
        config.maxBranches
    );
    const int trunkTopY = worldY + trunkHeight - 1;
    const int startY = worldY + static_cast<int>(std::round(static_cast<float>(trunkHeight) * 0.55f));
    const int horizontalBias = giantTrunk ? 1 : 0;

    for (int i = 0; i < branchCount; ++i) {
        const float angleRoll = RandomFloat(worldX + i * 101, worldZ - i * 83);
        const float angle = angleRoll * 6.2831853f;
        const int length = RandomRange(
            worldX + i * 29,
            worldZ - i * 31,
            config.minBranchLength,
            config.maxBranchLength
        );
        const int endX = worldX + horizontalBias + static_cast<int>(std::round(std::cos(angle) * static_cast<float>(length)));
        const int endZ = worldZ + horizontalBias + static_cast<int>(std::round(std::sin(angle) * static_cast<float>(length)));
        const int endY = RandomRange(
            worldX - i * 37,
            worldZ + i * 41,
            startY,
            std::max(startY, trunkTopY + 2)
        );

        nodes.push_back(TreeNode{
            endX,
            endY,
            endZ,
            config.canopyRadius,
            config.canopyHeight
        });
    }

    nodes.push_back(TreeNode{
        worldX + horizontalBias,
        trunkTopY - 2,
        worldZ + horizontalBias,
        config.canopyRadius + (giantTrunk ? 1 : 0),
        config.canopyHeight + 1
    });
    nodes.push_back(TreeNode{
        worldX + horizontalBias,
        trunkTopY + 1,
        worldZ + horizontalBias,
        std::max(1, config.canopyRadius - 1),
        2
    });

    return nodes;
}

void ConnectBranches(TreeBuildContext& ctx, int worldX, int worldY, int worldZ, int trunkHeight,
                     const std::vector<TreeNode>& nodes, bool giantTrunk) {
    const int trunkTopY = worldY + trunkHeight - 1;
    const int trunkX = worldX + (giantTrunk ? 1 : 0);
    const int trunkZ = worldZ + (giantTrunk ? 1 : 0);

    for (const TreeNode& node : nodes) {
        if (node.y > trunkTopY + 2) continue;
        const int attachY = std::clamp(node.y - 1, worldY + trunkHeight / 2, trunkTopY);
        if (attachY < worldY + static_cast<int>(std::round(static_cast<float>(trunkHeight) * 0.35f))) continue;
        PlaceBranchLine(ctx, trunkX, attachY, trunkZ, node.x, node.y, node.z);
    }
}

void GenerateOakBlob(int worldX, int worldY, int worldZ, int trunkHeight, int leafRadius, int foliageHeight,
                     const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    if (!bsr) return;

    TreeBuildContext ctx = MakeTreeContext(bsr, out);
    PlaceStraightTrunk(ctx, worldX, worldY, worldZ, trunkHeight);

    const TreeNode lowerNode{worldX, worldY + trunkHeight - 3, worldZ, leafRadius, foliageHeight};
    const TreeNode crownNode{worldX, worldY + trunkHeight, worldZ, std::max(1, leafRadius - 1), 2};
    PlaceBlobFoliage(ctx, lowerNode, true);
    PlaceBlobFoliage(ctx, crownNode, true);
}

void GenerateTallOak(int worldX, int worldY, int worldZ, const TreeConfig& config,
                     const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    if (!bsr) return;

    TreeBuildContext ctx = MakeTreeContext(bsr, out);
    const int trunkHeight = ConfigHeight(config, worldX, worldZ);
    const int visibleTrunkHeight = std::max(4, static_cast<int>(std::round(static_cast<float>(trunkHeight) * 0.72f)));
    PlaceStraightTrunk(ctx, worldX, worldY, worldZ, visibleTrunkHeight);

    std::vector<TreeNode> nodes = GenerateFoliageNodes(worldX, worldY, worldZ, visibleTrunkHeight, config, false);
    ConnectBranches(ctx, worldX, worldY, worldZ, visibleTrunkHeight, nodes, false);
    PlaceFancyFoliage(ctx, nodes, true);
}

void GenerateGiantOak(int worldX, int worldY, int worldZ, const TreeConfig& config,
                      const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    if (!bsr) return;

    TreeBuildContext ctx = MakeTreeContext(bsr, out);
    const int trunkHeight = ConfigHeight(config, worldX, worldZ);
    PlaceGiantTrunk(ctx, worldX, worldY, worldZ, trunkHeight);

    std::vector<TreeNode> nodes = GenerateFoliageNodes(worldX, worldY, worldZ, trunkHeight, config, true);
    ConnectBranches(ctx, worldX, worldY, worldZ, trunkHeight, nodes, true);
    PlaceFancyFoliage(ctx, nodes, true);
}

void GenerateOrnamentalOak(int worldX, int worldY, int worldZ,
                           const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    if (!bsr) return;

    const TreeConfig config{5, 8, 1, 2, 2, 3, 2, 3};
    GenerateOakBlob(worldX, worldY, worldZ, ConfigHeight(config, worldX, worldZ), 3, 3, bsr, out);
}

void GenerateTreeForBiome(BiomeType biome, int worldX, int worldY, int worldZ,
                          const blockstate::BlockStateRegistry* bsr, std::vector<WorldBlockPlacement>& out) {
    const float roll = RandomFloat(worldX, worldZ);

    switch (biome) {
        case BiomeType::FlowerForest: {
            const TreeConfig ornamental{5, 8, 1, 2, 2, 3, 2, 3};
            const TreeConfig smallFancy{6, 9, 2, 3, 2, 4, 2, 3};
            const TreeConfig tallFancy{8, 11, 3, 4, 3, 5, 2, 3};
            if (roll < 0.42f) {
                GenerateOrnamentalOak(worldX, worldY, worldZ, bsr, out);
            } else if (roll < 0.82f) {
                GenerateOakBlob(worldX, worldY, worldZ, ConfigHeight(ornamental, worldX + 13, worldZ - 17), 2, 3, bsr, out);
            } else {
                GenerateTallOak(worldX, worldY, worldZ, roll < 0.92f ? smallFancy : tallFancy, bsr, out);
            }
            break;
        }

        case BiomeType::Forest: {
            const TreeConfig mediumOak{7, 10, 2, 4, 2, 4, 2, 3};
            const TreeConfig tallOak{10, 15, 3, 5, 3, 5, 3, 3};
            const TreeConfig giantOak{16, 22, 4, 7, 4, 6, 3, 4};
            if (roll < 0.28f) {
                GenerateOakBlob(worldX, worldY, worldZ, ConfigHeight(mediumOak, worldX, worldZ), 2, 3, bsr, out);
            } else if (roll < 0.72f) {
                GenerateTallOak(worldX, worldY, worldZ, tallOak, bsr, out);
            } else {
                GenerateGiantOak(worldX, worldY, worldZ, giantOak, bsr, out);
            }
            break;
        }

        default:
            break;
    }
}

} // namespace

void TerrainGenerator::GenerateTreeInto(int worldX, int worldY, int worldZ,
                                        const blockstate::BlockStateRegistry* bsr,
                                        std::vector<WorldBlockPlacement>& out) {
    const TerrainParams params = GetTerrainParams();
    const TerrainColumnSample sample = SampleTerrainColumn(worldX, worldZ, params);
    GenerateTreeForBiome(
        sample.dominantBiome,
        worldX,
        worldY,
        worldZ,
        bsr,
        out
    );
}
