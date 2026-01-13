#pragma once

#include <string>

#include <ASCIIgL/util/MathUtil.hpp>

#include <glm/glm.hpp>

constexpr int CHUNK_SIZE = 16;
constexpr int REGION_SIZE = 32;

struct RegionCoord {
    int32_t x, y, z;

    RegionCoord() : x(0), y(0), z(0) {}
    RegionCoord(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    RegionCoord(const glm::ivec3& pos) : x(static_cast<int32_t>(pos.x)), y(static_cast<int32_t>(pos.y)), z(static_cast<int32_t>(pos.z)) {}

    bool operator==(const RegionCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const RegionCoord& other) const {
        return !(*this == other);
    }

    RegionCoord operator+(const RegionCoord& other) const {
        return RegionCoord(x + other.x, y + other.y, z + other.z);
    }

    RegionCoord operator-(const RegionCoord& other) const {
        return RegionCoord(x - other.x, y - other.y, z - other.z);
    }

    std::string ToString() const {
        return "(" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")";
    }
};

// Chunk coordinates (world space divided by chunk size)
struct ChunkCoord {
    int32_t x, y, z;
    
    ChunkCoord() : x(0), y(0), z(0) {}
    ChunkCoord(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    ChunkCoord(const glm::ivec3& pos) : x(static_cast<int32_t>(pos.x)), y(static_cast<int32_t>(pos.y)), z(static_cast<int32_t>(pos.z)) {}

    glm::ivec3 ToVec3() const {
        return glm::ivec3(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
    }
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const ChunkCoord& other) const {
        return !(*this == other);
    }

    ChunkCoord operator+(const ChunkCoord& other) const {
        return ChunkCoord(x + other.x, y + other.y, z + other.z);
    }

    ChunkCoord operator-(const ChunkCoord& other) const {
        return ChunkCoord(x - other.x, y - other.y, z - other.z);
    }

    std::string ToString() const {
        return "(" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")";
    }

    RegionCoord ToRegionCoord() const {
        return RegionCoord(
            ASCIIgL::MathUtil::FloorDivNegInf(x, REGION_SIZE),
            ASCIIgL::MathUtil::FloorDivNegInf(y, REGION_SIZE),
            ASCIIgL::MathUtil::FloorDivNegInf(z, REGION_SIZE)
        );
    }

    glm::ivec3 ToLocalRegion(const RegionCoord& r) const {
        return glm::ivec3(
            x - (r.x * REGION_SIZE),
            y - (r.y * REGION_SIZE),
            z - (r.z * REGION_SIZE)
        );
    }
};

// World position (block coordinates)
struct WorldCoord {
    int32_t x, y, z;
    
    WorldCoord() : x(0), y(0), z(0) {}
    WorldCoord(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    WorldCoord(const glm::ivec3& pos) : x(static_cast<int32_t>(pos.x)), y(static_cast<int32_t>(pos.y)), z(static_cast<int32_t>(pos.z)) {}
    
    glm::ivec3 ToVec3() const {
        return glm::ivec3(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
    }

    bool operator==(const WorldCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const WorldCoord& other) const {
        return !(*this == other);
    }

    WorldCoord operator+(const WorldCoord& other) const {
        return WorldCoord(x + other.x, y + other.y, z + other.z);
    }

    WorldCoord operator-(const WorldCoord& other) const {
        return WorldCoord(x - other.x, y - other.y, z - other.z);
    }

    std::string ToString() const {
        return "(" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")";
    }

    
    ChunkCoord ToChunkCoord() const {
        return ChunkCoord(
            x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE,
            y >= 0 ? y / CHUNK_SIZE : (y - CHUNK_SIZE + 1) / CHUNK_SIZE,
            z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE
        );
    }

    glm::ivec3 ToLocalChunkPos() const {
        return glm::ivec3(
            ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
            ((y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
            ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE
        );
    }
};

// splitmix64 mixer
static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

namespace std {
template<>
struct hash<ChunkCoord> {
    size_t operator()(ChunkCoord const& c) const noexcept {
        // cast to unsigned to avoid sign-extension
        uint64_t x = static_cast<uint64_t>(static_cast<int64_t>(c.x));
        uint64_t y = static_cast<uint64_t>(static_cast<int64_t>(c.y));
        uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(c.z));

        // mix coordinates with splitmix64
        uint64_t h = splitmix64(x + 0x9e3779b97f4a7c15ULL);
        h ^= splitmix64(y + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        h ^= splitmix64(z + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));

        return static_cast<size_t>(h);
    }
};

template<>
struct hash<RegionCoord> {
    size_t operator()(RegionCoord const& c) const noexcept {
        // cast to unsigned to avoid sign-extension
        uint64_t x = static_cast<uint64_t>(static_cast<int64_t>(c.x));
        uint64_t y = static_cast<uint64_t>(static_cast<int64_t>(c.y));
        uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(c.z));

        // mix coordinates with splitmix64
        uint64_t h = splitmix64(x + 0x9e3779b97f4a7c15ULL);
        h ^= splitmix64(y + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        h ^= splitmix64(z + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));

        return static_cast<size_t>(h);
    }
};
}
