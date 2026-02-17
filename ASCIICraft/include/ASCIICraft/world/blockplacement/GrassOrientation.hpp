#pragma once

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/blockstate/BlockFace.hpp>

// Deterministic pseudo-random facing based on world position.
// Returns one of: BlockFace::North / South / East / West.
inline BlockFace GetGrassFacingForPosition(int x, int y, int z) {
    // Simple 32-bit hash of integer coordinates (handles negatives via wraparound).
    auto hashCoord = [](int v) {
        // Large odd constants to mix bits; cast to unsigned for wraparound behavior.
        return static_cast<uint32_t>(v * 73856093);
    };

    uint32_t h = 2166136261u; // FNV offset basis
    h ^= hashCoord(x); h *= 16777619u;
    h ^= hashCoord(y); h *= 16777619u;
    h ^= hashCoord(z); h *= 16777619u;

    static constexpr BlockFace FACES[4] = {
        BlockFace::North, BlockFace::South, BlockFace::East, BlockFace::West
    };
    return FACES[h & 3u];
}

inline BlockFace GetGrassFacingForPosition(const WorldCoord& pos) {
    return GetGrassFacingForPosition(pos.x, pos.y, pos.z);
}

