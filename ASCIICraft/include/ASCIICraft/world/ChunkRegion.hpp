#pragma once

#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include <mutex>
#include <optional>
#include <list>
#include <memory>

#include <ASCIICraft/world/Chunk.hpp>
#include <ASCIICraft/world/Coords.hpp>
#include <ASCIICraft/world/CrossChunkEdit.hpp>

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
    uint32_t stateId;
    bool operator==(const SerializedBlock& other) const {
        return stateId == other.stateId;
    }
};

struct SerializedEdit {
    uint32_t stateId;
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
            return std::hash<uint32_t>{}(b.stateId);
        }
    };
}

static constexpr uint32_t MAX_CHUNK_BLOB_SIZE = 1u << 20; // 1 MiB
static constexpr uint32_t MAX_META_BLOB_SIZE  = 1u << 20; // 1 MiB

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

    RegionFile(const RegionFile&) = delete;
    RegionFile& operator=(const RegionFile&) = delete;

    RegionFile(RegionFile&&) = delete;
    RegionFile& operator=(RegionFile&&) = delete;

    bool LoadChunk(Chunk* out);
    bool SaveChunk(const Chunk* data);

    bool LoadMetaData(const ChunkCoord& pos, MetaBucket* out);
    bool SaveMetaData(const ChunkCoord& pos, const MetaBucket* data);

    /// Used by unload callback: save chunk + optional meta under region lock, then optionally Close(). Thread-safe.
    void SaveChunkForUnload(const Chunk* data, const ChunkCoord& pos, const MetaBucket* meta, bool closeAfter);

    /// Batch save: open once, write multiple chunks/metas, flush once. Much faster than N separate SaveChunk/SaveMetaData.
    bool BeginBatchSave();
    void SaveChunkInBatch(const Chunk* data);
    void SaveMetaDataInBatch(const ChunkCoord& pos, const MetaBucket* data);
    void EndBatchSave();

    const RegionCoord& GetRegionCoord() const;
    const std::string& GetPath() const;

    /// Flush and close. Caller must hold _mutex (e.g. from unload callback or EndBatchSave).
    void Close();
    bool IsFileOpen() const { return _file.is_open(); }
    std::unique_lock<std::mutex> Lock() { return std::unique_lock<std::mutex>(_mutex); }

private:
    mutable std::mutex _mutex;
    std::optional<std::unique_lock<std::mutex>> _batchLock;  // held from BeginBatchSave until EndBatchSave
    std::fstream _file;
    std::string _path;
    const RegionCoord _coord;

    /// Open file if not open (read-write), read header/index if file exists. Caller must hold _mutex.
    bool EnsureOpen();

    RegionHeader header{};
    std::vector<ChunkIndexEntry> chunkIndexes;
    std::vector<MetaBucketIndexEntry> metaIndexes;

    uint32_t indexOffset(const glm::ivec3& lc) const {
        return static_cast<uint32_t>(lc.x
             + lc.y * REGION_SIZE
             + lc.z * REGION_SIZE * REGION_SIZE);
    }

    bool openForRead();
    bool openForReadWrite();

    void writeHeaderAndIndex();
    void readHeaderAndIndex();

    /// Append one chunk/meta blob and update in-memory index. Caller must have file open (e.g. openForReadWrite or BeginBatchSave).
    void appendChunkBlobAndUpdateIndex(const Chunk* data);
    void appendMetaBlobAndUpdateIndex(const ChunkCoord& pos, const MetaBucket* data);

    void parseChunkBlob(const std::vector<uint8_t>& blob, Chunk* out);
    std::vector<uint8_t> buildChunkBlob(const Chunk* data);

    void parseMetaBlob(const std::vector<uint8_t>& blob, MetaBucket* out);
    std::vector<uint8_t> buildMetaBlob(const MetaBucket* data);
};

class RegionManager {
public:
    RegionManager() = default;
    ~RegionManager() = default;

    static constexpr int MAX_REGIONS = 32;

    void AddRegion(const RegionCoord& coord);
    void RemoveRegion(const RegionCoord& coord);
    std::shared_ptr<RegionFile> AccessRegion(const RegionCoord& coord);
    bool FilePresent(const RegionCoord& coord);
    /// Thread-safe: return existing region or create and return new one. Keeps region alive until shared_ptr is released.
    std::shared_ptr<RegionFile> GetOrCreate(const RegionCoord& coord);

private:
    mutable std::mutex mutex_;
    using RegionList = std::list<std::shared_ptr<RegionFile>>;
    std::unordered_map<RegionCoord, RegionList::iterator> regionFiles;
    RegionList regionList;
};
