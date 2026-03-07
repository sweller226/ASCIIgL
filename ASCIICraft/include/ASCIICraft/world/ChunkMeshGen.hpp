#pragma once

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>

/// Mesh data produced by chunk mesh generation (shared by Chunk::GenerateMesh and ChunkJobQueue).
/// Main thread creates ASCIIgL::Mesh from this and assigns to the chunk (with the appropriate TextureArray*).
struct ChunkMeshData {
    std::vector<std::byte> opaqueVertices;
    std::vector<int> opaqueIndices;
    std::vector<std::byte> transparentVertices;
    std::vector<int> transparentIndices;

    bool HasOpaque() const {
        return !opaqueVertices.empty() && !opaqueIndices.empty();
    }
    bool HasTransparent() const {
        return !transparentVertices.empty() && !transparentIndices.empty();
    }
};

/// Build mesh data from chunk and neighbor block arrays (read-only).
/// Used by Chunk::GenerateMesh (synchronous) and ChunkJobQueue (worker tasks).
ChunkMeshData BuildChunkMeshData(
    ChunkCoord coord,
    const uint32_t* chunkBlocks,
    const std::array<const uint32_t*, 6>& neighborBlocks,
    const blockstate::BlockStateRegistry* bsr
);
