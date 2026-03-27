#pragma once

#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/blockstate/FaceDir.hpp>

// Deterministic pseudo-random facing based on world position and seed.
// Returns one of: FaceDir::North / South / East / West.
inline FaceDir GetGrassFacingForPosition(int x, int y, int z, uint32_t seed = 0) {
    // Simple 32-bit hash of integer coordinates + seed (handles negatives via wraparound).
    auto hashCoord = [](int v) {
        return static_cast<uint32_t>(v * 73856093);
    };

    uint32_t h = 2166136261u; // FNV offset basis
    h ^= static_cast<uint32_t>(seed); h *= 16777619u;
    h ^= hashCoord(x); h *= 16777619u;
    h ^= hashCoord(y); h *= 16777619u;
    h ^= hashCoord(z); h *= 16777619u;

    static constexpr FaceDir FACES[4] = {
        FaceDir::North, FaceDir::South, FaceDir::East, FaceDir::West
    };
    return FACES[h & 3u];
}

inline FaceDir GetGrassFacingForPosition(const WorldCoord& pos, uint32_t seed = 0) {
    return GetGrassFacingForPosition(pos.x, pos.y, pos.z, seed);
}

