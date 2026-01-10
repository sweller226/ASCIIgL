#pragma once

#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <stdexcept>

#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/CrossChunkEdit.hpp>

// Adjust to your region dimensions (chunks per axis)
constexpr int REGION_SIZE = 16;

#pragma pack(push, 1)

// Region-level metadata
struct RegionHeader {
    uint32_t version;    // Region file format version
    uint16_t chunkCount; // number of present chunks in this region
    uint32_t metaStart;  // start of the meta data index section
    uint32_t chunkStart; // start of the chunk data index section
};

struct MetaBucketIndexEntry {
    uint32_t packedCoord;  // local coord for meta bucket chunk in region
    uint32_t offset;       // Byte offset from file start
    uint32_t length;       // Compressed chunk data length
    uint8_t  flags;        // bit0 = present, bit1 = dirty, others reserved
};

// One entry per possible chunk in the region
struct ChunkIndexEntry {
    uint32_t offset;       // Byte offset from file start
    uint32_t length;       // Compressed chunk data length
    uint8_t  flags;        // bit0 = present, bit1 = dirty, others reserved
};

struct MetaBucketHeader {
    uint32_t version;
};

// Chunk-level metadata
struct ChunkHeader {
    uint32_t version;   // Chunk format version (palette, lighting, etc.)
};

// One unique block entry in the palette
struct SerializedBlock {
    uint8_t type;
    uint8_t  metadata;
    bool operator==(const SerializedBlock& other) const {
        return type == other.type && metadata == other.metadata;
    }
};

struct SerializedEdit {
    uint8_t type;
    uint8_t  metadata;
    uint16_t pos;
};

struct PaletteHeader {
    uint16_t paletteSize; // Number of SerializedBlock entries
    uint8_t  indexBits;   // 4, 8, or 16 bits per block index
};

#pragma pack(pop)

namespace std {
    template<>
    struct hash<SerializedBlock> {
        size_t operator()(const SerializedBlock& b) const {
            return (static_cast<size_t>(b.type) << 8) ^ b.metadata;
        }
    };
}

// Region file interface
class RegionFile {
/*
    todo:
        Right now chunks and metadata is just appended at the end when needed
        There are old chunk blobs that are left over when the same chunk is unloaded
        twice. There needs to be a routine to go through and optimize the region file.
        This will be slow, so it should be done with a button if the player wants... maybe.
*/ 

public:
    explicit RegionFile(const RegionCoord& coord);
    ~RegionFile();

    bool LoadChunk(Chunk* out);
    bool SaveChunk(const Chunk* data);

    bool LoadMetaData(const ChunkCoord& pos, MetaBucket* out);
    bool SaveMetaData(const ChunkCoord& pos, const MetaBucket* data);

    const RegionCoord& GetRegionCoord() const;
    const std::string& GetPath() const;

private:
    std::fstream _file;
    std::string _path;
    const RegionCoord _coord;

    RegionHeader header{};
    std::vector<ChunkIndexEntry> chunkIndexes;
    std::vector<MetaBucketIndexEntry> metaIndexes;

    uint32_t indexOffset(const glm::ivec3& lc) const {
        return static_cast<uint32_t>(lc.x
             + lc.y * REGION_SIZE
             + lc.z * REGION_SIZE * REGION_SIZE);
    }

    void writeHeaderAndIndex();
    void readHeaderAndIndex();

    void parseChunkBlob(const std::vector<uint8_t>& blob, Chunk* out);
    std::vector<uint8_t> buildChunkBlob(const Chunk* data);

    void parseMetaBlob(const std::vector<uint8_t>& blob, MetaBucket* out);
    std::vector<uint8_t> buildMetaBlob(const MetaBucket* data);
};

class RegionManager {
public:
    RegionManager() = default;
    ~RegionManager() = default; // default is fine; list elements are destroyed automatically

    static constexpr int MAX_REGIONS = 32; 

    void AddRegion(RegionFile rf);
    void RemoveRegion(const RegionCoord& coord);
    RegionFile& AccessRegion(const RegionCoord& coord);
    bool FilePresent(const RegionCoord& coord);

private:
    std::unordered_map<RegionCoord, std::list<RegionFile>::iterator> regionFiles;
    std::list<RegionFile> regionList;
};
