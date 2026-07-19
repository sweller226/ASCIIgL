#include <ASCIICraft/world/chunk/ChunkRegion.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/VariantKey.hpp>
#include <ASCIICraft/world/chunk/LegacyStateIdMigration.hpp>

#include <filesystem>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

#include <sstream>

#include <ASCIIgL/util/Logger.hpp>

namespace {

void AppendBytes(std::vector<uint8_t>& buffer, const void* ptr, size_t size) {
    const size_t old = buffer.size();
    buffer.resize(old + size);
    std::memcpy(buffer.data() + old, ptr, size);
}

void AppendLengthPrefixedString(std::vector<uint8_t>& buffer, const std::string& s) {
    if (s.size() > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("Serialized string exceeds uint16 length");
    }
    const uint16_t len = static_cast<uint16_t>(s.size());
    AppendBytes(buffer, &len, sizeof(len));
    if (len > 0) {
        AppendBytes(buffer, s.data(), len);
    }
}

std::string ReadLengthPrefixedString(const std::vector<uint8_t>& blob, size_t& pos) {
    if (pos + sizeof(uint16_t) > blob.size()) {
        throw std::runtime_error("Chunk/meta blob truncated (string length)");
    }
    uint16_t len = 0;
    std::memcpy(&len, blob.data() + pos, sizeof(len));
    pos += sizeof(len);
    if (pos + len > blob.size()) {
        throw std::runtime_error("Chunk/meta blob truncated (string data)");
    }
    std::string out;
    if (len > 0) {
        out.assign(reinterpret_cast<const char*>(blob.data() + pos), len);
        pos += len;
    }
    return out;
}

bool TryParseMetaBlobV2(
    const std::vector<uint8_t>& blob,
    MetaBucket* out,
    const blockstate::BlockStateRegistry& bsr
) {
    size_t pos = 0;
    auto require = [&](size_t n) -> bool {
        return pos + n <= blob.size();
    };

    if (!require(sizeof(MetaBucketHeader))) return false;
    MetaBucketHeader mh{};
    std::memcpy(&mh, blob.data() + pos, sizeof(mh));
    pos += sizeof(mh);
    if (mh.version != META_BLOB_VERSION_V2) return false;

    if (!require(sizeof(uint32_t))) return false;
    uint32_t count = 0;
    std::memcpy(&count, blob.data() + pos, sizeof(count));
    pos += sizeof(count);
    // Reject absurd counts early so a v1 blob with leading dword == 2 falls back cleanly.
    if (count > (blob.size() / 4u)) return false;

    out->edits.clear();
    out->edits.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!require(sizeof(uint16_t))) return false;
        uint16_t packedPos = 0;
        std::memcpy(&packedPos, blob.data() + pos, sizeof(packedPos));
        pos += sizeof(packedPos);

        if (!require(sizeof(uint16_t))) return false;
        uint16_t nameLen = 0;
        std::memcpy(&nameLen, blob.data() + pos, sizeof(nameLen));
        pos += sizeof(nameLen);
        if (!require(nameLen)) return false;
        std::string name;
        if (nameLen > 0) {
            name.assign(reinterpret_cast<const char*>(blob.data() + pos), nameLen);
            pos += nameLen;
        }

        if (!require(sizeof(uint16_t))) return false;
        uint16_t propsLen = 0;
        std::memcpy(&propsLen, blob.data() + pos, sizeof(propsLen));
        pos += sizeof(propsLen);
        if (!require(propsLen)) return false;
        std::string props;
        if (propsLen > 0) {
            props.assign(reinterpret_cast<const char*>(blob.data() + pos), propsLen);
            pos += propsLen;
        }

        CrossChunkEdit e;
        e.packedPos = packedPos;
        e.stateId = blockstate::ResolveStateFromSerialized(bsr, name, props);
        out->edits.push_back(e);
    }
    return true;
}

} // namespace

// Small helpers used by the methods below
static inline uint64_t fileSizeOnDisk(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return 0;
    return static_cast<uint64_t>(in.tellg());
}

static void safeRead(std::fstream& f, char* buffer, std::streamsize count, uint64_t fileSize, uint64_t offset) {
    if (!f.is_open()) throw std::runtime_error("safeRead: file not open");
    if (count < 0) throw std::runtime_error("safeRead: negative count");

    // IMPORTANT: do not call read with count == 0 on MSVC debug CRT (it asserts buffer != nullptr)
    if (count == 0) {
        ASCIIgL::Logger::Warningf("safeRead: requested count is 0 (offset=%llu)", static_cast<unsigned long long>(offset));
        return;
    }

    if (static_cast<uint64_t>(offset) + static_cast<uint64_t>(count) > fileSize) {
        ASCIIgL::Logger::Errorf("safeRead: read would go past EOF (offset=%llu, count=%lld, fileSize=%llu)",
                                static_cast<unsigned long long>(offset),
                                static_cast<long long>(count),
                                static_cast<unsigned long long>(fileSize));
        throw std::runtime_error("safeRead: read past EOF");
    }

    if (buffer == nullptr) {
        ASCIIgL::Logger::Errorf("safeRead: buffer is nullptr (count=%lld, offset=%llu)", static_cast<long long>(count), static_cast<unsigned long long>(offset));
        throw std::runtime_error("safeRead: null buffer");
    }

    f.read(buffer, count);
    if (f.gcount() != count) {
        ASCIIgL::Logger::Errorf("safeRead: short read (got=%lld, expected=%lld)", static_cast<long long>(f.gcount()), static_cast<long long>(count));
        throw std::runtime_error("safeRead: short read");
    }
}


static void safeWrite(std::fstream& f, const char* buffer, std::streamsize count) {
    if (!f.is_open()) throw std::runtime_error("safeWrite: file not open");
    f.write(buffer, count);
    if (!f.good()) {
        ASCIIgL::Logger::Error("safeWrite: write failed");
        throw std::runtime_error("safeWrite: write failed");
    }
    f.flush();
    if (!f.good()) {
        ASCIIgL::Logger::Error("safeWrite: flush failed");
        throw std::runtime_error("safeWrite: flush failed");
    }
}

RegionFile::RegionFile(const RegionCoord& coord) : _coord(coord) {
    chunkIndexes.resize(static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE);
    metaIndexes.resize(static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE);

    const std::filesystem::path regionDir = "regions";
    if (!std::filesystem::exists(regionDir)) {
        std::filesystem::create_directories(regionDir);
    }

    std::string filename = "r_" + std::to_string(coord.x) + "." + std::to_string(coord.y) + "." + std::to_string(coord.z);
    _path = (regionDir / filename).string();

    header.version = 1;
    header.chunkCount = 0;
    header.chunkStart = 0;
    header.metaStart = 0;
    std::fill(chunkIndexes.begin(), chunkIndexes.end(), ChunkIndexEntry{0, 0, 0});
    std::fill(metaIndexes.begin(), metaIndexes.end(), MetaBucketIndexEntry{0, 0, 0, 0});
    // File is not opened here; opened on first use via EnsureOpen(), closed when last chunk in region is unloaded.
}

RegionFile::~RegionFile() {
    std::lock_guard<std::mutex> g(_mutex);
    if (_file.is_open()) {
        _file.flush();
        _file.close();
    }
}

bool RegionFile::EnsureOpen() {
    if (_file.is_open()) return true;
    bool exists = std::filesystem::exists(_path);
    if (!exists) {
        _file.open(_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!_file.is_open()) {
            ASCIIgL::Logger::Errorf("RegionFile::EnsureOpen failed to create: %s", _path.c_str());
            return false;
        }
        const size_t entryCount = static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE;
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        header.chunkStart = headerSize + chunkIndexTableSize + metaIndexTableSize;
        header.metaStart = header.chunkStart;
        writeHeaderAndIndex();
        return true;
    }
    _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!_file.is_open()) {
        ASCIIgL::Logger::Errorf("RegionFile::EnsureOpen failed to open: %s", _path.c_str());
        return false;
    }
    readHeaderAndIndex();
    return true;
}

void RegionFile::Close() {
    if (_file.is_open()) {
        _file.flush();
        _file.close();
    }
}

void RegionFile::readHeaderAndIndex() {
    _file.seekg(0, std::ios::beg);
    if (!_file.good()) throw std::runtime_error("Region _file not readable: " + _path);

    const size_t entryCount = static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE;

    // Read header
    _file.read(reinterpret_cast<char*>(&header), static_cast<std::streamsize>(sizeof(header)));
    if (_file.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        // File too small to contain a valid header; treat as empty region
        header.version = 1;
        header.chunkCount = 0;

        // Compute sensible defaults for starts (header + both chunkIndexes tables)
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        header.chunkStart = headerSize + chunkIndexTableSize + metaIndexTableSize;
        header.metaStart  = header.chunkStart;

        std::fill(chunkIndexes.begin(), chunkIndexes.end(), ChunkIndexEntry{0, 0, 0});
        std::fill(metaIndexes.begin(), metaIndexes.end(), MetaBucketIndexEntry{0, 0, 0, 0});
        return;
    }

    // Read chunk index table (immediately after header)
    const size_t chunkIndexBytesNeeded = entryCount * sizeof(ChunkIndexEntry);
    _file.read(reinterpret_cast<char*>(chunkIndexes.data()), static_cast<std::streamsize>(chunkIndexBytesNeeded));
    std::streamsize chunkIndexBytesRead = _file.gcount();
    if (static_cast<size_t>(chunkIndexBytesRead) < chunkIndexBytesNeeded) {
        size_t entriesRead = static_cast<size_t>(chunkIndexBytesRead) / sizeof(ChunkIndexEntry);
        for (size_t i = entriesRead; i < chunkIndexes.size(); ++i) {
            chunkIndexes[i] = ChunkIndexEntry{0, 0, 0};
        }
        // If the _file is truncated, we cannot reliably read meta index; set defaults and return.
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        header.chunkStart = headerSize + chunkIndexTableSize + metaIndexTableSize;
        header.metaStart  = header.chunkStart;
        std::fill(metaIndexes.begin(), metaIndexes.end(), MetaBucketIndexEntry{0, 0, 0, 0});
        return;
    }

    // Read meta index table (follows chunk index table)
    const size_t metaIndexBytesNeeded = entryCount * sizeof(MetaBucketIndexEntry);
    _file.read(reinterpret_cast<char*>(metaIndexes.data()), static_cast<std::streamsize>(metaIndexBytesNeeded));
    std::streamsize metaIndexBytesRead = _file.gcount();
    if (static_cast<size_t>(metaIndexBytesRead) < metaIndexBytesNeeded) {
        size_t entriesRead = static_cast<size_t>(metaIndexBytesRead) / sizeof(MetaBucketIndexEntry);
        for (size_t i = entriesRead; i < metaIndexes.size(); ++i) {
            metaIndexes[i] = MetaBucketIndexEntry{0, 0, 0, 0};
        }
    }

    // Sanity: if header fields are zero or obviously wrong, compute sensible defaults
    const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
    const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
    const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
    const uint32_t expectedChunkStart = headerSize + chunkIndexTableSize + metaIndexTableSize;
    if (header.chunkStart == 0) header.chunkStart = expectedChunkStart;
    if (header.metaStart == 0) header.metaStart = header.chunkStart;
}

void RegionFile::writeHeaderAndIndex() {
    _file.seekp(0, std::ios::beg);
    if (!_file.good()) throw std::runtime_error("Region _file not writable: " + _path);

    // Ensure header.chunkStart/metaStart are set consistently before writing.
    const size_t entryCount = static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE;
    const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
    const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
    const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
    // If caller hasn't set chunkStart/metaStart, compute defaults
    if (header.chunkStart == 0) header.chunkStart = headerSize + chunkIndexTableSize + metaIndexTableSize;
    if (header.metaStart == 0) header.metaStart = header.chunkStart;

    // Write header
    _file.write(reinterpret_cast<const char*>(&header), static_cast<std::streamsize>(sizeof(header)));

    // Write chunk index table
    _file.write(reinterpret_cast<const char*>(chunkIndexes.data()), static_cast<std::streamsize>(chunkIndexes.size() * sizeof(ChunkIndexEntry)));

    // Write meta index table
    _file.write(reinterpret_cast<const char*>(metaIndexes.data()), static_cast<std::streamsize>(metaIndexes.size() * sizeof(MetaBucketIndexEntry)));

    _file.flush();

    if (!_file.good()) throw std::runtime_error("Failed to write header/index to region _file: " + _path);
}


// Helper: unpack indices (supports 4,8,16 bits)
static void unpackIndices(const uint8_t* data, size_t dataSize, uint8_t indexBits,
                          size_t count, std::vector<uint16_t>& out) {
    out.resize(count);
    size_t pos = 0;
    if (indexBits == 8) {
        if (dataSize < count) throw std::runtime_error("truncated indices");
        for (size_t i = 0; i < count; ++i) out[i] = data[pos++];
    } else if (indexBits == 16) {
        if (dataSize < count * 2) throw std::runtime_error("truncated indices");
        for (size_t i = 0; i < count; ++i) {
            uint16_t v = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            out[i] = v;
        }
    } else if (indexBits == 4) {
        size_t needed = (count + 1) / 2;
        if (dataSize < needed) throw std::runtime_error("truncated indices");
        size_t idx = 0;
        for (size_t b = 0; b < needed; ++b) {
            uint8_t byte = data[b];
            uint8_t low = byte & 0x0F;
            uint8_t high = (byte >> 4) & 0x0F;
            out[idx++] = low;
            if (idx < count) out[idx++] = high;
        }
    } else {
        throw std::runtime_error("unsupported indexBits");
    }
}

// Helper: pack indices for palette (supports 4,8,16 bits)
static std::vector<uint8_t> packIndices(const std::vector<uint16_t>& indices, uint8_t indexBits) {
    std::vector<uint8_t> out;
    if (indexBits == 8) {
        out.resize(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) out[i] = static_cast<uint8_t>(indices[i] & 0xFF);
    } else if (indexBits == 16) {
        out.resize(indices.size() * 2);
        size_t p = 0;
        for (uint16_t v : indices) {
            out[p++] = static_cast<uint8_t>(v & 0xFF);
            out[p++] = static_cast<uint8_t>((v >> 8) & 0xFF);
        }
    } else if (indexBits == 4) {
        size_t needed = (indices.size() + 1) / 2;
        out.assign(needed, 0);
        size_t p = 0;
        for (size_t i = 0; i < indices.size(); i += 2) {
            uint8_t low = static_cast<uint8_t>(indices[i] & 0x0F);
            uint8_t high = 0;
            if (i + 1 < indices.size()) high = static_cast<uint8_t>(indices[i + 1] & 0x0F);
            out[p++] = static_cast<uint8_t>((high << 4) | low);
        }
    } else {
        throw std::runtime_error("unsupported indexBits");
    }
    return out;
}

void RegionFile::parseChunkBlob(
    const std::vector<uint8_t>& blob,
    Chunk* out,
    const blockstate::BlockStateRegistry& bsr
) {
    size_t pos = 0;
    auto require = [&](size_t n) {
        if (pos + n > blob.size()) throw std::runtime_error("Chunk blob truncated");
    };

    require(sizeof(ChunkHeader));
    ChunkHeader ch;
    std::memcpy(&ch, blob.data() + pos, sizeof(ch));
    pos += sizeof(ch);

    require(sizeof(PaletteHeader));
    PaletteHeader ph;
    std::memcpy(&ph, blob.data() + pos, sizeof(ph));
    pos += sizeof(ph);

    if (ph.paletteSize > 65535) throw std::runtime_error("paletteSize too large");

    std::vector<uint32_t> resolvedPalette;
    resolvedPalette.reserve(ph.paletteSize);

    if (ch.version == CHUNK_BLOB_VERSION_V2) {
        for (uint16_t i = 0; i < ph.paletteSize; ++i) {
            const std::string name = ReadLengthPrefixedString(blob, pos);
            const std::string props = ReadLengthPrefixedString(blob, pos);
            resolvedPalette.push_back(blockstate::ResolveStateFromSerialized(bsr, name, props));
        }
    } else if (ch.version == CHUNK_BLOB_VERSION_V1 || ch.version == 0) {
        // v1: raw numeric stateIds (version 0 tolerated for truncated/default headers)
        require(static_cast<size_t>(ph.paletteSize) * sizeof(SerializedBlock));
        for (uint16_t i = 0; i < ph.paletteSize; ++i) {
            SerializedBlock sb{};
            std::memcpy(&sb, blob.data() + pos, sizeof(sb));
            pos += sizeof(sb);
            resolvedPalette.push_back(legacy_state_id::RemapLegacyStateId(sb.stateId));
        }
    } else {
        throw std::runtime_error("Unsupported chunk blob version: " + std::to_string(ch.version));
    }

    size_t indicesBytes = blob.size() - pos;
    const uint8_t* indicesPtr = blob.data() + pos;

    std::vector<uint16_t> indices;
    unpackIndices(indicesPtr, indicesBytes, ph.indexBits, static_cast<size_t>(Chunk::VOLUME), indices);

    for (size_t i = 0; i < indices.size(); ++i) {
        uint16_t idx = indices[i];
        if (idx >= resolvedPalette.size()) throw std::runtime_error("palette index out of range");
        out->SetBlockStateByIndex(static_cast<int>(i), resolvedPalette[idx]);
    }
}

std::vector<uint8_t> RegionFile::buildChunkBlob(
    const Chunk* data,
    const blockstate::BlockStateRegistry& bsr
) {
    std::unordered_map<uint32_t, uint16_t> paletteMap;
    std::vector<uint32_t> palette;
    std::vector<uint16_t> indices(Chunk::VOLUME);

    for (int i = 0; i < static_cast<int>(Chunk::VOLUME); ++i) {
        const uint32_t stateId = data->GetBlockStateByIndex(i);
        auto it = paletteMap.find(stateId);
        if (it == paletteMap.end()) {
            const uint16_t id = static_cast<uint16_t>(palette.size());
            paletteMap.emplace(stateId, id);
            palette.push_back(stateId);
            indices[i] = id;
        } else {
            indices[i] = it->second;
        }
    }

    if (palette.size() > 65535) throw std::runtime_error("Palette too large (unexpected)");

    uint8_t indexBits = 8;
    if (palette.size() <= 16) indexBits = 4;
    else if (palette.size() <= 256) indexBits = 8;
    else indexBits = 16;

    const uint32_t maxIndex = (indexBits == 16) ? 0xFFFFu : ((1u << indexBits) - 1u);
    if (palette.size() - 1u > maxIndex) throw std::runtime_error("Palette size doesn't fit in chosen indexBits");

    std::vector<uint8_t> buffer;

    ChunkHeader ch{ CHUNK_BLOB_VERSION_V2 };
    AppendBytes(buffer, &ch, sizeof(ch));

    PaletteHeader ph{ static_cast<uint16_t>(palette.size()), indexBits };
    AppendBytes(buffer, &ph, sizeof(ph));

    for (const uint32_t stateId : palette) {
        const blockstate::SerializedStateIdentity id = blockstate::SerializeState(bsr, stateId);
        AppendLengthPrefixedString(buffer, id.name);
        AppendLengthPrefixedString(buffer, id.props);
    }

    std::vector<uint8_t> packed = packIndices(indices, indexBits);
    if (!packed.empty()) AppendBytes(buffer, packed.data(), packed.size());

    if (buffer.empty()) {
        ASCIIgL::Logger::Warning("buildChunkBlob: buffer is empty, nothing to write");
    }

    if (buffer.size() > MAX_CHUNK_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("buildChunkBlob: buffer size %zu exceeds MAX_CHUNK_BLOB_SIZE %u", buffer.size(), MAX_CHUNK_BLOB_SIZE);
        throw std::runtime_error("Chunk blob too large");
    }

    return buffer;
}

bool RegionFile::LoadChunk(Chunk* out, const blockstate::BlockStateRegistry& bsr) {
    std::lock_guard<std::mutex> g(_mutex);
    if (!EnsureOpen()) {
        ASCIIgL::Logger::Error("LoadChunk: EnsureOpen failed");
        return false;
    }

    RegionCoord rp = out->GetCoord().ToRegionCoord();
    glm::ivec3 lp = out->GetCoord().ToLocalRegion(rp);

    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= sizes::REGION_SIZE || lp.y >= sizes::REGION_SIZE || lp.z >= sizes::REGION_SIZE) {
        ASCIIgL::Logger::Warning("LoadChunk: local coords out of bounds");
        return false;
    }

    uint32_t off = indexOffset(lp);
    if (off >= chunkIndexes.size()) {
        ASCIIgL::Logger::Warning("LoadChunk: indexOffset out of chunkIndexes range");
        return false;
    }

    auto& entry = chunkIndexes[off];
    if (!(entry.flags & 0x1) || entry.length == 0) return false;
    if (entry.length > MAX_CHUNK_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("LoadChunk: chunk length %u exceeds MAX_CHUNK_BLOB_SIZE %u", entry.length, MAX_CHUNK_BLOB_SIZE);
        return false;
    }

    _file.seekg(0, std::ios::end);
    uint64_t fsize = static_cast<uint64_t>(_file.tellg());
    if (entry.offset > fsize || static_cast<uint64_t>(entry.offset) + entry.length > fsize) {
        ASCIIgL::Logger::Errorf("LoadChunk: offset+length past EOF (offset=%llu, length=%u, fileSize=%llu)",
                                static_cast<unsigned long long>(entry.offset),
                                entry.length,
                                static_cast<unsigned long long>(fsize));
        return false;
    }

    _file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!_file.good()) {
        ASCIIgL::Logger::Errorf("LoadChunk: seekg failed at offset %llu", static_cast<unsigned long long>(entry.offset));
        throw std::runtime_error("Failed to seek region file for read");
    }

    std::vector<uint8_t> blob;
    try {
        blob.resize(entry.length);
    } catch (const std::bad_alloc&) {
        ASCIIgL::Logger::Errorf("LoadChunk: allocation failed for length %u", entry.length);
        return false;
    }
    if (blob.size() != entry.length) return false;

    safeRead(_file, reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()), fsize, entry.offset);
    parseChunkBlob(blob, out, bsr);
    return true;
}


void RegionFile::appendChunkBlobAndUpdateIndex(const Chunk* data, const blockstate::BlockStateRegistry& bsr) {
    RegionCoord rp = data->GetCoord().ToRegionCoord();
    glm::ivec3 lp = data->GetCoord().ToLocalRegion(rp);
    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= sizes::REGION_SIZE || lp.y >= sizes::REGION_SIZE || lp.z >= sizes::REGION_SIZE) {
        ASCIIgL::Logger::Error("appendChunkBlobAndUpdateIndex: local coords out of bounds");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }
    uint32_t off = indexOffset(lp);
    if (off >= chunkIndexes.size()) {
        ASCIIgL::Logger::Error("appendChunkBlobAndUpdateIndex: index out of range");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }
    auto& entry = chunkIndexes[off];
    std::vector<uint8_t> raw = buildChunkBlob(data, bsr);
    _file.clear();
    _file.seekp(0, std::ios::end);
    if (!_file.good()) {
        ASCIIgL::Logger::Error("appendChunkBlobAndUpdateIndex: seekp failed");
        throw std::runtime_error("Failed to seek region file for write");
    }
    std::streampos p = _file.tellp();
    uint32_t offset;
    if (p == static_cast<std::streampos>(-1)) {
        std::error_code ec;
        auto sz = std::filesystem::file_size(_path, ec);
        if (ec) {
            ASCIIgL::Logger::Error("appendChunkBlobAndUpdateIndex: tellp failed and could not get file size");
            throw std::runtime_error("tellp failed and could not get file size");
        }
        if (sz > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            ASCIIgL::Logger::Error("appendChunkBlobAndUpdateIndex: file size exceeds 4GB");
            throw std::runtime_error("Region file too large (>4GB)");
        }
        offset = static_cast<uint32_t>(sz);
        ASCIIgL::Logger::Debugf("appendChunkBlobAndUpdateIndex: tellp failed, using file_size fallback (%u)", offset);
    } else {
        offset = static_cast<uint32_t>(p);
    }
    safeWrite(_file, reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    if (!(entry.flags & 0x1)) header.chunkCount++;
    entry.offset = offset;
    entry.length = static_cast<uint32_t>(raw.size());
    entry.flags = static_cast<uint8_t>(entry.flags | 0x1);
}

bool RegionFile::SaveChunk(const Chunk* data, const blockstate::BlockStateRegistry& bsr) {
    std::lock_guard<std::mutex> g(_mutex);
    if (!EnsureOpen()) {
        ASCIIgL::Logger::Error("SaveChunk: EnsureOpen failed");
        throw std::runtime_error("Failed to open region file for write");
    }
    appendChunkBlobAndUpdateIndex(data, bsr);
    writeHeaderAndIndex();
    _file.flush();
    return true;
}


bool RegionFile::LoadMetaData(const ChunkCoord& pos, MetaBucket* out, const blockstate::BlockStateRegistry& bsr) {
    std::lock_guard<std::mutex> g(_mutex);
    if (!EnsureOpen()) {
        ASCIIgL::Logger::Error("LoadMetaData: EnsureOpen failed");
        return false;
    }

    RegionCoord rp = pos.ToRegionCoord();
    glm::ivec3 lp = pos.ToLocalRegion(rp);

    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= sizes::REGION_SIZE || lp.y >= sizes::REGION_SIZE || lp.z >= sizes::REGION_SIZE) {
        ASCIIgL::Logger::Warning("LoadMetaData: local coords out of bounds");
        return false;
    }

    uint32_t off = indexOffset(lp);
    if (off >= metaIndexes.size()) {
        ASCIIgL::Logger::Warning("LoadMetaData: indexOffset out of metaIndexes range");
        return false;
    }

    auto& entry = metaIndexes[off];
    if (!(entry.flags & 0x1) || entry.length == 0) return false;
    if (entry.length > MAX_META_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("LoadMetaData: metadata length %u exceeds MAX_META_BLOB_SIZE %u",
                                entry.length, MAX_META_BLOB_SIZE);
        return false;
    }

    _file.seekg(0, std::ios::end);
    uint64_t fsize = static_cast<uint64_t>(_file.tellg());
    if (entry.offset > fsize || static_cast<uint64_t>(entry.offset) + entry.length > fsize) {
        ASCIIgL::Logger::Errorf("LoadMetaData: offset+length past EOF (offset=%llu, length=%u, fileSize=%llu)",
                                static_cast<unsigned long long>(entry.offset),
                                entry.length,
                                static_cast<unsigned long long>(fsize));
        return false;
    }

    _file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!_file.good()) {
        ASCIIgL::Logger::Errorf("LoadMetaData: seekg failed at offset %llu", static_cast<unsigned long long>(entry.offset));
        throw std::runtime_error("Failed to seek region file for read");
    }

    std::vector<uint8_t> blob;
    try {
        blob.resize(entry.length);
    } catch (const std::bad_alloc&) {
        ASCIIgL::Logger::Errorf("LoadMetaData: allocation failed for length %u", entry.length);
        return false;
    }
    if (blob.size() != entry.length) return false;

    safeRead(_file, reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()), fsize, entry.offset);
    parseMetaBlob(blob, out, bsr);
    return true;
}

void RegionFile::appendMetaBlobAndUpdateIndex(
    const ChunkCoord& pos,
    const MetaBucket* data,
    const blockstate::BlockStateRegistry& bsr
) {
    RegionCoord rp = pos.ToRegionCoord();
    glm::ivec3 lp = pos.ToLocalRegion(rp);
    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= sizes::REGION_SIZE || lp.y >= sizes::REGION_SIZE || lp.z >= sizes::REGION_SIZE) {
        ASCIIgL::Logger::Error("appendMetaBlobAndUpdateIndex: local coords out of bounds");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }
    uint32_t off = indexOffset(lp);
    if (off >= metaIndexes.size()) {
        ASCIIgL::Logger::Error("appendMetaBlobAndUpdateIndex: index out of range");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }
    auto& entry = metaIndexes[off];
    std::vector<uint8_t> raw = buildMetaBlob(data, bsr);
    if (header.metaStart == 0) {
        const size_t entryCount = static_cast<size_t>(sizes::REGION_SIZE) * sizes::REGION_SIZE * sizes::REGION_SIZE;
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        header.chunkStart = header.chunkStart ? header.chunkStart : (headerSize + chunkIndexTableSize + metaIndexTableSize);
        header.metaStart = header.chunkStart;
    }
    _file.clear();
    _file.seekp(0, std::ios::end);
    if (!_file.good()) {
        ASCIIgL::Logger::Error("appendMetaBlobAndUpdateIndex: seekp failed");
        throw std::runtime_error("Failed to seek region file for write");
    }
    std::streampos p = _file.tellp();
    uint32_t offset;
    if (p == static_cast<std::streampos>(-1)) {
        std::error_code ec;
        auto sz = std::filesystem::file_size(_path, ec);
        if (ec) {
            ASCIIgL::Logger::Error("appendMetaBlobAndUpdateIndex: tellp failed and could not get file size");
            throw std::runtime_error("tellp failed and could not get file size");
        }
        if (sz > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            ASCIIgL::Logger::Error("appendMetaBlobAndUpdateIndex: file size exceeds 4GB");
            throw std::runtime_error("Region file too large (>4GB)");
        }
        offset = static_cast<uint32_t>(sz);
        ASCIIgL::Logger::Debugf("appendMetaBlobAndUpdateIndex: tellp failed, using file_size fallback (%u)", offset);
    } else {
        uint64_t offset64 = static_cast<uint64_t>(p);
        if (offset64 > std::numeric_limits<uint32_t>::max()) {
            ASCIIgL::Logger::Error("appendMetaBlobAndUpdateIndex: file offset exceeds 4GB limit");
            throw std::runtime_error("Region file too large (>4GB)");
        }
        offset = static_cast<uint32_t>(offset64);
    }
    safeWrite(_file, reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    entry.packedCoord = off;
    entry.offset = offset;
    entry.length = static_cast<uint32_t>(raw.size());
    entry.flags = static_cast<uint8_t>(entry.flags | 0x1);
}

bool RegionFile::SaveMetaData(
    const ChunkCoord& pos,
    const MetaBucket* data,
    const blockstate::BlockStateRegistry& bsr
) {
    std::lock_guard<std::mutex> g(_mutex);
    if (!EnsureOpen()) {
        ASCIIgL::Logger::Error("SaveMetaData: EnsureOpen failed");
        throw std::runtime_error("Failed to open region file for write");
    }
    appendMetaBlobAndUpdateIndex(pos, data, bsr);
    writeHeaderAndIndex();
    _file.flush();
    return true;
}

void RegionFile::SaveChunkForUnload(
    const Chunk* data,
    const ChunkCoord& pos,
    const MetaBucket* meta,
    bool closeAfter,
    const blockstate::BlockStateRegistry& bsr
) {
    std::lock_guard<std::mutex> g(_mutex);
    if (!EnsureOpen()) {
        ASCIIgL::Logger::Error("SaveChunkForUnload: EnsureOpen failed");
        throw std::runtime_error("Failed to open region file for write");
    }
    appendChunkBlobAndUpdateIndex(data, bsr);
    if (meta && !meta->edits.empty())
        appendMetaBlobAndUpdateIndex(pos, meta, bsr);
    writeHeaderAndIndex();
    _file.flush();
    if (closeAfter)
        Close();
}

bool RegionFile::BeginBatchSave() {
    _batchLock.emplace(_mutex);
    return EnsureOpen();
}

void RegionFile::SaveChunkInBatch(const Chunk* data, const blockstate::BlockStateRegistry& bsr) {
    appendChunkBlobAndUpdateIndex(data, bsr);
}

void RegionFile::SaveMetaDataInBatch(
    const ChunkCoord& pos,
    const MetaBucket* data,
    const blockstate::BlockStateRegistry& bsr
) {
    appendMetaBlobAndUpdateIndex(pos, data, bsr);
}

void RegionFile::EndBatchSave() {
    writeHeaderAndIndex();
    _file.flush();
    Close();
    _batchLock.reset();
}

void RegionFile::parseMetaBlob(
    const std::vector<uint8_t>& blob,
    MetaBucket* out,
    const blockstate::BlockStateRegistry& bsr
) {
    if (!out) throw std::invalid_argument("out is null");
    out->edits.clear();

    if (blob.size() < sizeof(uint32_t)) return;

    // Prefer v2 when the leading dword is the version marker and the payload walks cleanly.
    // Falls back to v1 when count happens to equal 2 but the blob is still the old numeric layout.
    if (blob.size() >= sizeof(MetaBucketHeader) + sizeof(uint32_t)) {
        uint32_t maybeVersion = 0;
        std::memcpy(&maybeVersion, blob.data(), sizeof(maybeVersion));
        if (maybeVersion == META_BLOB_VERSION_V2) {
            MetaBucket probe;
            if (TryParseMetaBlobV2(blob, &probe, bsr)) {
                out->edits = std::move(probe.edits);
                return;
            }
        }
    }

    // v1: raw count + SerializedEdit entries (legacy numeric stateIds)
    size_t pos = 0;
    uint32_t count = 0;
    std::memcpy(&count, blob.data() + pos, sizeof(count));
    pos += sizeof(count);

    const size_t perEntry = sizeof(SerializedEdit);
    size_t availableEntries = 0;
    if (pos < blob.size()) {
        availableEntries = (blob.size() - pos) / perEntry;
    }
    if (count > availableEntries) count = static_cast<uint32_t>(availableEntries);

    out->edits.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        SerializedEdit se;
        std::memcpy(&se, blob.data() + pos, perEntry);
        pos += perEntry;

        CrossChunkEdit e;
        e.packedPos = se.pos;
        e.stateId = legacy_state_id::RemapLegacyStateId(se.stateId);
        out->edits.push_back(e);
    }
}

std::vector<uint8_t> RegionFile::buildMetaBlob(
    const MetaBucket* data,
    const blockstate::BlockStateRegistry& bsr
) {
    if (!data) return {};

    const uint32_t count = static_cast<uint32_t>(data->edits.size());
    std::vector<uint8_t> out;
    out.reserve(sizeof(MetaBucketHeader) + sizeof(uint32_t) + count * 32u);

    MetaBucketHeader mh{ META_BLOB_VERSION_V2 };
    AppendBytes(out, &mh, sizeof(mh));
    AppendBytes(out, &count, sizeof(count));

    for (const CrossChunkEdit& e : data->edits) {
        AppendBytes(out, &e.packedPos, sizeof(e.packedPos));
        const blockstate::SerializedStateIdentity id = blockstate::SerializeState(bsr, e.stateId);
        AppendLengthPrefixedString(out, id.name);
        AppendLengthPrefixedString(out, id.props);
    }

    if (out.empty()) {
        ASCIIgL::Logger::Warning("buildMetaBlob: buffer is empty, nothing to write");
    }
    if (out.size() > MAX_META_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("buildMetaBlob: buffer size %zu exceeds MAX_META_BLOB_SIZE %u", out.size(), MAX_META_BLOB_SIZE);
        throw std::runtime_error("Meta blob too large");
    }

    return out;
}

const RegionCoord& RegionFile::GetRegionCoord() const {
    return _coord;
}

const std::string& RegionFile::GetPath() const {
    return _path;
}

void RegionManager::AddRegion(const RegionCoord& coord) {
    std::lock_guard<std::mutex> g(mutex_);
    regionList.push_front(std::make_shared<RegionFile>(coord));
    regionFiles.emplace(coord, regionList.begin());

    if (regionList.size() <= static_cast<size_t>(MAX_REGIONS)) return;

    auto lastIt = std::prev(regionList.end());
    RegionCoord lastCoord = (*lastIt)->GetRegionCoord();
    {
        auto regionLock = (*lastIt)->Lock();
        if (!(*lastIt)->IsFileOpen()) {
            regionLock.unlock();
            regionList.erase(lastIt);
            regionFiles.erase(lastCoord);
        }
    }
}

void RegionManager::RemoveRegion(const RegionCoord& coord) {
    std::lock_guard<std::mutex> g(mutex_);
    auto it = regionFiles.find(coord);
    if (it == regionFiles.end()) return;
    regionList.erase(it->second);
    regionFiles.erase(it);
}

std::shared_ptr<RegionFile> RegionManager::AccessRegion(const RegionCoord& coord) {
    std::lock_guard<std::mutex> g(mutex_);
    auto it = regionFiles.find(coord);
    if (it == regionFiles.end())
        throw std::runtime_error("Region not loaded");
    regionList.splice(regionList.begin(), regionList, it->second);
    it->second = regionList.begin();
    return *(it->second);
}

bool RegionManager::FilePresent(const RegionCoord& coord) {
    std::lock_guard<std::mutex> g(mutex_);
    return regionFiles.find(coord) != regionFiles.end();
}

std::shared_ptr<RegionFile> RegionManager::GetOrCreate(const RegionCoord& coord) {
    std::lock_guard<std::mutex> g(mutex_);
    auto it = regionFiles.find(coord);
    if (it != regionFiles.end()) {
        regionList.splice(regionList.begin(), regionList, it->second);
        it->second = regionList.begin();
        return *(it->second);
    }
    regionList.push_front(std::make_shared<RegionFile>(coord));
    regionFiles.emplace(coord, regionList.begin());

    if (regionList.size() > static_cast<size_t>(MAX_REGIONS)) {
        auto lastIt = std::prev(regionList.end());
        RegionCoord lastCoord = (*lastIt)->GetRegionCoord();
        auto regionLock = (*lastIt)->Lock();
        if (!(*lastIt)->IsFileOpen()) {
            regionLock.unlock();
            regionList.erase(lastIt);
            regionFiles.erase(lastCoord);
        }
    }
    return *regionList.begin();
}