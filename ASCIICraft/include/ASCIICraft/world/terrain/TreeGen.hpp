#pragma once

#include <vector>

#include <ASCIICraft/world/terrain/TerrainGenerator.hpp>   // BiomeType
#include <ASCIICraft/world/terrain/TerrainResult.hpp>      // WorldBlockPlacement

namespace blockstate { class BlockStateRegistry; }

namespace terrain {

/// Builds one tree at (worldX, worldY, worldZ), appending its blocks to \p out in
/// absolute world coordinates.
///
/// Pure and deterministic: the species, height, branch angles and canopy shape derive
/// entirely from (biome, worldX, worldZ) via RandomFloat. No noise sampling, no world
/// seed, no TerrainGenerator instance - which is what makes tree geometry unit
/// testable on its own.
///
/// Blocks are NOT clipped to any chunk. A tree rooted near a chunk edge legitimately
/// emits positions in neighbouring chunks; distributing them is the caller's job
/// (TerrainResult::crossChunkBlocks).
void GenerateTree(BiomeType biome,
                  int worldX, int worldY, int worldZ,
                  const blockstate::BlockStateRegistry& bsr,
                  std::vector<WorldBlockPlacement>& out);

} // namespace terrain
