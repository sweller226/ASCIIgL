#include <ASCIICraft/world/ChunkRegion.hpp>

#include <filesystem>
#include <algorithm>
#include <cassert>
#include <limits>

#include <sstream>

#include <ASCIIgL/util/Logger.hpp>

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
    // Ensure chunkIndexes vector is sized before any reads/writes
    chunkIndexes.resize(static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE);
    metaIndexes.resize(static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE);  // ADD THIS LINE

    const std::filesystem::path regionDir = "regions";
    if (!std::filesystem::exists(regionDir)) {
        std::filesystem::create_directories(regionDir);
    }

    std::string filename = "r_" + std::to_string(coord.x) + "." + std::to_string(coord.y) + "." + std::to_string(coord.z);

    _path = (regionDir / filename).string();

    bool exists = std::filesystem::exists(_path);
    if (!exists) {
        _file.open(_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!_file.is_open()) {
            throw std::runtime_error("Failed to create region file: " + _path);
        }

        header.version = 1;
        header.chunkCount = 0;

        // Initialize chunkIndexes entries to zero
        std::fill(chunkIndexes.begin(), chunkIndexes.end(), ChunkIndexEntry{0, 0, 0});

        writeHeaderAndIndex();
    }
    else {
        _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!_file.is_open()) {
            throw std::runtime_error("Failed to open region file: " + _path);
        }

        readHeaderAndIndex();
    }

    _file.close();
}

RegionFile::~RegionFile() {
    if (_file.is_open()) {
        _file.flush();
        _file.close();
    }
}

void RegionFile::readHeaderAndIndex() {
    _file.seekg(0, std::ios::beg);
    if (!_file.good()) throw std::runtime_error("Region _file not readable: " + _path);

    const size_t entryCount = static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE;

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
    const size_t entryCount = static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE;
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

void RegionFile::parseChunkBlob(const std::vector<uint8_t>& blob, Chunk* out) {
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

    // Sanity check palette size (avoid huge allocations)
    if (ph.paletteSize > 65535) throw std::runtime_error("paletteSize too large");

    // Read palette
    std::vector<SerializedBlock> palette;
    palette.resize(ph.paletteSize);
    if (ph.paletteSize > 0) {
        require(palette.size() * sizeof(SerializedBlock));
        std::memcpy(palette.data(), blob.data() + pos, palette.size() * sizeof(SerializedBlock));
        pos += palette.size() * sizeof(SerializedBlock);
    }

    // Remaining bytes are indices
    size_t indicesBytes = blob.size() - pos;
    const uint8_t* indicesPtr = blob.data() + pos;

    std::vector<uint16_t> indices;
    unpackIndices(indicesPtr, indicesBytes, ph.indexBits, static_cast<size_t>(Chunk::VOLUME), indices);

    // Map indices to blocks and fill chunk
    for (size_t i = 0; i < indices.size(); ++i) {
        uint16_t idx = indices[i];
        if (idx >= palette.size()) throw std::runtime_error("palette index out of range");
        const SerializedBlock& sb = palette[idx];
        Block b;
        b.type = static_cast<BlockType>(sb.type);
        b.metadata = sb.metadata;
        out->SetBlockByIndex(static_cast<int>(i), b);
    }
}

std::vector<uint8_t> RegionFile::buildChunkBlob(const Chunk* data) {
    // Use uint16_t for map values to avoid accidental overflow and to match indices vector
    std::unordered_map<SerializedBlock, uint16_t> paletteMap;
    std::vector<SerializedBlock> palette;
    std::vector<uint16_t> indices(Chunk::VOLUME);

    for (int i = 0; i < static_cast<int>(Chunk::VOLUME); ++i) {
        Block b = data->GetBlockByIndex(i);
        SerializedBlock key{ static_cast<uint8_t>(b.type), b.metadata };
        auto it = paletteMap.find(key);
        if (it == paletteMap.end()) {
            uint16_t id = static_cast<uint16_t>(palette.size());
            paletteMap.emplace(key, id);
            palette.push_back(key);
            indices[i] = id;
        } else {
            indices[i] = it->second;
        }
    }

    // Enforce a reasonable palette limit (you said <=256 block types per chunk)
    if (palette.size() > 65535) throw std::runtime_error("Palette too large (unexpected)");

    uint8_t indexBits = 8;
    if (palette.size() <= 16) indexBits = 4;
    else if (palette.size() <= 256) indexBits = 8;
    else indexBits = 16;

    // Defensive check: ensure palette fits in chosen indexBits
    uint32_t maxIndex = (indexBits == 16) ? 0xFFFFu : ((1u << indexBits) - 1u);
    if (palette.size() - 1u > maxIndex) throw std::runtime_error("Palette size doesn't fit in chosen indexBits");

    std::vector<uint8_t> buffer;
    auto append = [&](const void* ptr, size_t size) {
        size_t old = buffer.size();
        buffer.resize(old + size);
        std::memcpy(buffer.data() + old, ptr, size);
    };

    ChunkHeader ch{ 1 };
    append(&ch, sizeof(ch));

    PaletteHeader ph{ static_cast<uint16_t>(palette.size()), indexBits };
    append(&ph, sizeof(ph));

    if (!palette.empty()) append(palette.data(), palette.size() * sizeof(SerializedBlock));

    std::vector<uint8_t> packed = packIndices(indices, indexBits);
    if (!packed.empty()) append(packed.data(), packed.size());

    return buffer;
}

bool RegionFile::LoadChunk(Chunk* out) {
    RegionCoord rp = out->GetCoord().ToRegionCoord();
    glm::ivec3 lp = out->GetCoord().ToLocalRegion(rp);

    ASCIIgL::Logger::Debugf("LoadChunk: rp=(%d,%d,%d), lp=(%d,%d,%d)", rp.x, rp.y, rp.z, lp.x, lp.y, lp.z);

    // Validate local coords
    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        ASCIIgL::Logger::Warning("LoadChunk: local coords out of bounds");
        return false;
    }

    uint32_t off = indexOffset(lp);
    ASCIIgL::Logger::Debugf("LoadChunk: indexOffset = %u", off);

    if (off >= chunkIndexes.size()) {
        ASCIIgL::Logger::Warning("LoadChunk: indexOffset out of chunkIndexes range");
        return false;
    }

    auto& entry = chunkIndexes[off];
    ASCIIgL::Logger::Debugf("LoadChunk: entry.flags=%u, entry.offset=%llu, entry.length=%u",
                            static_cast<unsigned int>(entry.flags),
                            static_cast<unsigned long long>(entry.offset),
                            entry.length);

    if (!(entry.flags & 0x1)) {
        ASCIIgL::Logger::Debug("LoadChunk: chunk not present (flags)");
        return false;
    }

    if (entry.length == 0) {
        ASCIIgL::Logger::Debug("LoadChunk: chunk length is zero");
        return false;
    }

    if (entry.length > MAX_CHUNK_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("LoadChunk: chunk length %u exceeds MAX_CHUNK_BLOB_SIZE %u", entry.length, MAX_CHUNK_BLOB_SIZE);
        return false;
    }

    uint64_t fsize = fileSizeOnDisk(_path);
    if (entry.offset > fsize || static_cast<uint64_t>(entry.offset) + entry.length > fsize) {
        ASCIIgL::Logger::Errorf("LoadChunk: offset+length past EOF (offset=%llu, length=%u, fileSize=%llu)",
                                static_cast<unsigned long long>(entry.offset),
                                entry.length,
                                static_cast<unsigned long long>(fsize));
        return false;
    }

    if (!openForRead()) {
        ASCIIgL::Logger::Error("LoadChunk: openForRead failed");
        return false;
    }

    _file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!_file.good()) {
        ASCIIgL::Logger::Errorf("LoadChunk: seekg failed at offset %llu", static_cast<unsigned long long>(entry.offset));
        _file.close();
        throw std::runtime_error("Failed to seek region file for read");
    }

    std::vector<uint8_t> blob;
    try {
        blob.resize(entry.length);
    } catch (const std::bad_alloc&) {
        ASCIIgL::Logger::Errorf("LoadChunk: allocation failed for length %u", entry.length);
        _file.close();
        return false;
    }
    if (blob.empty() && entry.length != 0) {
        ASCIIgL::Logger::Errorf("LoadChunk: allocation produced empty buffer for length %u", entry.length);
        _file.close();
        return false;
    }

    // after allocating blob (and after verifying entry.length <= MAX_... and file size checks)
    if (blob.size() == 0) {
        ASCIIgL::Logger::Errorf("LoadChunk: blob.size() == 0 after allocation (entry.length=%u)", entry.length);
        _file.close();
        return false;
    }
    if (blob.data() == nullptr) {
        ASCIIgL::Logger::Errorf("LoadChunk: blob.data() == nullptr (entry.length=%u)", entry.length);
        _file.close();
        return false;
    }

    safeRead(_file, reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()), fsize, entry.offset);

    ASCIIgL::Logger::Debugf("LoadChunk: read %zu bytes OK", blob.size());

    parseChunkBlob(blob, out);

    _file.close();
    ASCIIgL::Logger::Info("LoadChunk: success");
    return true;
}


bool RegionFile::SaveChunk(const Chunk* data) {
    RegionCoord rp = data->GetCoord().ToRegionCoord();
    glm::ivec3 lp = data->GetCoord().ToLocalRegion(rp);

    ASCIIgL::Logger::Debugf("SaveChunk: rp=(%d,%d,%d), lp=(%d,%d,%d)", rp.x, rp.y, rp.z, lp.x, lp.y, lp.z);

    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        ASCIIgL::Logger::Error("SaveChunk: local coords out of bounds");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }

    uint32_t off = indexOffset(lp);
    ASCIIgL::Logger::Debugf("SaveChunk: indexOffset = %u", off);

    if (off >= chunkIndexes.size()) {
        ASCIIgL::Logger::Error("SaveChunk: indexOffset out of chunkIndexes range");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }

    auto& entry = chunkIndexes[off];

    std::vector<uint8_t> raw = buildChunkBlob(data);
    ASCIIgL::Logger::Debugf("SaveChunk: built blob of %zu bytes", raw.size());

    if (raw.empty()) {
        ASCIIgL::Logger::Warning("SaveChunk: blob size is zero, nothing to write");
        return false;
    }
    if (raw.size() > MAX_CHUNK_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("SaveChunk: blob size %zu exceeds MAX_CHUNK_BLOB_SIZE %u", raw.size(), MAX_CHUNK_BLOB_SIZE);
        throw std::runtime_error("Chunk blob too large");
    }

    if (!openForReadWrite()) {
        ASCIIgL::Logger::Error("SaveChunk: openForReadWrite failed");
        throw std::runtime_error("Failed to open region file for write");
    }

    // append to EOF
    _file.seekp(0, std::ios::end);
    if (!_file.good()) {
        ASCIIgL::Logger::Error("SaveChunk: seekp failed");
        _file.close();
        throw std::runtime_error("Failed to seek region file for write");
    }

    std::streampos p = _file.tellp();
    if (p == static_cast<std::streampos>(-1)) {
        ASCIIgL::Logger::Error("SaveChunk: tellp failed");
        _file.close();
        throw std::runtime_error("tellp failed");
    }

    uint32_t offset = static_cast<uint32_t>(p);
    ASCIIgL::Logger::Debugf("SaveChunk: writing at offset %u", offset);

    safeWrite(_file, reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));

    if (!(entry.flags & 0x1)) {
        header.chunkCount++;
        ASCIIgL::Logger::Debugf("SaveChunk: incremented chunkCount to %u", header.chunkCount);
    }

    entry.offset = offset;
    entry.length = static_cast<uint32_t>(raw.size());
    entry.flags = static_cast<uint8_t>(entry.flags | 0x1);

    ASCIIgL::Logger::Debugf("SaveChunk: updated index entry (offset=%u, length=%u, flags=%u)",
                            entry.offset, entry.length, static_cast<unsigned int>(entry.flags));

    // Persist header and index tables (must be robust/atomic in writeHeaderAndIndex)
    writeHeaderAndIndex();

    _file.close();
    ASCIIgL::Logger::Info("SaveChunk: success");
    return true;
}


bool RegionFile::LoadMetaData(const ChunkCoord& pos, MetaBucket* out) {
    RegionCoord rp = pos.ToRegionCoord();
    glm::ivec3 lp = pos.ToLocalRegion(rp);

    ASCIIgL::Logger::Debugf("LoadMetaData: rp=(%d,%d,%d), lp=(%d,%d,%d)",
                             rp.x, rp.y, rp.z, lp.x, lp.y, lp.z);

    // Validate local coords
    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        ASCIIgL::Logger::Warning("LoadMetaData: local coords out of bounds");
        return false;
    }

    uint32_t off = indexOffset(lp);
    ASCIIgL::Logger::Debugf("LoadMetaData: indexOffset = %u", off);

    if (off >= metaIndexes.size()) {
        ASCIIgL::Logger::Warning("LoadMetaData: indexOffset out of metaIndexes range");
        return false;
    }

    auto& entry = metaIndexes[off];
    ASCIIgL::Logger::Debugf("LoadMetaData: entry.flags=%u, entry.offset=%llu, entry.length=%u",
                            static_cast<unsigned int>(entry.flags),
                            static_cast<unsigned long long>(entry.offset),
                            entry.length);

    if (!(entry.flags & 0x1)) {
        ASCIIgL::Logger::Debug("LoadMetaData: metadata not present");
        return false;
    }

    if (entry.length == 0) {
        ASCIIgL::Logger::Debug("LoadMetaData: metadata length is zero");
        return false;
    }

    if (entry.length > MAX_META_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("LoadMetaData: metadata length %u exceeds MAX_META_BLOB_SIZE %u",
                                entry.length, MAX_META_BLOB_SIZE);
        return false;
    }

    // If header.metaStart is zero, log fallback but we still treat entry.offset as absolute.
    if (header.metaStart == 0) {
        ASCIIgL::Logger::Debug("LoadMetaData: metaStart missing, computing fallback (using absolute offsets)");
    }

    uint64_t fsize = fileSizeOnDisk(_path);
    if (entry.offset > fsize || static_cast<uint64_t>(entry.offset) + entry.length > fsize) {
        ASCIIgL::Logger::Errorf("LoadMetaData: offset+length past EOF (offset=%llu, length=%u, fileSize=%llu)",
                                static_cast<unsigned long long>(entry.offset),
                                entry.length,
                                static_cast<unsigned long long>(fsize));
        return false;
    }

    if (!openForRead()) {
        ASCIIgL::Logger::Error("LoadMetaData: openForRead failed");
        return false;
    }

    _file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!_file.good()) {
        ASCIIgL::Logger::Errorf("LoadMetaData: seekg failed at offset %llu", static_cast<unsigned long long>(entry.offset));
        _file.close();
        throw std::runtime_error("Failed to seek region file for read");
    }

    std::vector<uint8_t> blob;
    try {
        blob.resize(entry.length);
    } catch (const std::bad_alloc&) {
        ASCIIgL::Logger::Errorf("LoadMetaData: allocation failed for length %u", entry.length);
        _file.close();
        return false;
    }
    if (blob.empty() && entry.length != 0) {
        ASCIIgL::Logger::Errorf("LoadMetaData: allocation produced empty buffer for length %u", entry.length);
        _file.close();
        return false;
    }

    // after allocating blob (and after verifying entry.length <= MAX_... and file size checks)
    if (blob.size() == 0) {
        ASCIIgL::Logger::Errorf("LoadMetaData: blob.size() == 0 after allocation (entry.length=%u)", entry.length);
        _file.close();
        return false;
    }
    if (blob.data() == nullptr) {
        ASCIIgL::Logger::Errorf("LoadMetaData: blob.data() == nullptr (entry.length=%u)", entry.length);
        _file.close();
        return false;
    }


    safeRead(_file, reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()), fsize, entry.offset);

    ASCIIgL::Logger::Debugf("LoadMetaData: read %zu bytes OK", blob.size());

    parseMetaBlob(blob, out);

    _file.close();
    ASCIIgL::Logger::Info("LoadMetaData: success");
    return true;
}

bool RegionFile::SaveMetaData(const ChunkCoord& pos, const MetaBucket* data) {
    RegionCoord rp = pos.ToRegionCoord();
    glm::ivec3 lp = pos.ToLocalRegion(rp);

    ASCIIgL::Logger::Debugf("SaveMetaData: rp=(%d,%d,%d), lp=(%d,%d,%d)",
                             rp.x, rp.y, rp.z, lp.x, lp.y, lp.z);

    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        ASCIIgL::Logger::Error("SaveMetaData: local coords out of bounds");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }

    uint32_t off = indexOffset(lp);
    ASCIIgL::Logger::Debugf("SaveMetaData: indexOffset = %u", off);

    if (off >= metaIndexes.size()) {
        ASCIIgL::Logger::Error("SaveMetaData: indexOffset out of metaIndexes range");
        throw std::out_of_range("Local chunk coords out of region bounds");
    }

    auto& entry = metaIndexes[off];

    std::vector<uint8_t> raw = buildMetaBlob(data);
    ASCIIgL::Logger::Debugf("SaveMetaData: built blob of %zu bytes", raw.size());

    if (raw.empty()) {
        ASCIIgL::Logger::Warning("SaveMetaData: blob size is zero, nothing to write");
        return false;
    }
    if (raw.size() > MAX_META_BLOB_SIZE) {
        ASCIIgL::Logger::Errorf("SaveMetaData: meta blob size %zu exceeds MAX_META_BLOB_SIZE %u", raw.size(), MAX_META_BLOB_SIZE);
        throw std::runtime_error("Meta blob too large");
    }

    // Ensure header.metaStart is initialized (we keep absolute offsets)
    if (header.metaStart == 0) {
        const size_t entryCount = static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE;
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        header.chunkStart = header.chunkStart ? header.chunkStart : (headerSize + chunkIndexTableSize + metaIndexTableSize);
        header.metaStart = header.chunkStart;
        ASCIIgL::Logger::Debugf("SaveMetaData: initialized header.metaStart=%u", header.metaStart);
    }

    if (!openForReadWrite()) {
        ASCIIgL::Logger::Error("SaveMetaData: openForReadWrite failed");
        throw std::runtime_error("Failed to open region file for write");
    }

    _file.seekp(0, std::ios::end);
    if (!_file.good()) {
        ASCIIgL::Logger::Error("SaveMetaData: seekp failed");
        _file.close();
        throw std::runtime_error("Failed to seek region file for write");
    }

    std::streampos p = _file.tellp();
    if (p == static_cast<std::streampos>(-1)) {
        ASCIIgL::Logger::Error("SaveMetaData: tellp failed");
        _file.close();
        throw std::runtime_error("tellp failed");
    }

    uint64_t offset64 = static_cast<uint64_t>(p);
    
    // Check for overflow before assigning to uint32_t
    if (offset64 > std::numeric_limits<uint32_t>::max()) {
        ASCIIgL::Logger::Error("SaveMetaData: file offset exceeds 4GB limit");
        _file.close();
        throw std::runtime_error("Region file too large (>4GB)");
    }
    
    uint32_t offset = static_cast<uint32_t>(offset64);
    ASCIIgL::Logger::Debugf("SaveMetaData: writing at offset %u", offset);

    safeWrite(_file, reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));

    // Update meta index entry (store absolute offset)
    entry.packedCoord = off;
    entry.offset = offset;  // Now safe
    entry.length = static_cast<uint32_t>(raw.size());
    entry.flags = static_cast<uint8_t>(entry.flags | 0x1);

    ASCIIgL::Logger::Debugf("SaveMetaData: updated index entry (offset=%llu, length=%u, flags=%u)",
                            static_cast<unsigned long long>(entry.offset),
                            entry.length,
                            static_cast<unsigned int>(entry.flags));

    // Persist header + both index tables
    writeHeaderAndIndex();

    _file.close();
    ASCIIgL::Logger::Info("SaveMetaData: success");
    return true;
}


bool RegionFile::openForRead() {
    if (_file.is_open()) _file.close();
    _file.open(_path, std::ios::binary | std::ios::in);
    if (!_file.is_open()) {
        ASCIIgL::Logger::Errorf("RegionFile::openForRead failed: %s", _path.c_str());
        return false;
    }
    return true;
}

bool RegionFile::openForReadWrite() {
    if (_file.is_open()) _file.close();
    _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!_file.is_open()) {
        ASCIIgL::Logger::Errorf("RegionFile::openForReadWrite failed: %s", _path.c_str());
        return false;
    }
    return true;
}

void RegionFile::parseMetaBlob(const std::vector<uint8_t>& blob, MetaBucket* out) {
    if (!out) throw std::invalid_argument("out is null");
    out->edits.clear();

    size_t pos = 0;
    auto require = [&](size_t n) {
        if (pos + n > blob.size()) throw std::runtime_error("Meta blob truncated");
    };

    // Need at least 4 bytes for count (native endianness)
    if (blob.size() < sizeof(uint32_t)) return; // empty or invalid blob -> leave edits empty

    // Read count using native endianness (memcpy)
    uint32_t count = 0;
    std::memcpy(&count, blob.data() + pos, sizeof(count));
    pos += sizeof(count);

    // Each SerializedEdit is 4 bytes
    const size_t perEntry = sizeof(SerializedEdit);
    size_t availableEntries = 0;
    if (pos < blob.size()) {
        availableEntries = (blob.size() - pos) / perEntry;
    }

    // Clamp count to available entries to tolerate truncated blobs
    if (count > availableEntries) count = static_cast<uint32_t>(availableEntries);

    out->edits.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        SerializedEdit se;
        std::memcpy(&se, blob.data() + pos, perEntry);
        pos += perEntry;

        CrossChunkEdit e;
        e.packedPos = se.pos;                 // native endianness
        e.block.type = static_cast<BlockType>(se.type);
        e.block.metadata = se.metadata;

        out->edits.push_back(e);
    }
}

std::vector<uint8_t> RegionFile::buildMetaBlob(const MetaBucket* data) {
    if (!data) return {};

    const uint32_t count = static_cast<uint32_t>(data->edits.size());
    std::vector<uint8_t> out;
    out.reserve(sizeof(uint32_t) + count * sizeof(SerializedEdit));

    // Helper to append raw bytes
    auto append_raw = [&](const void* ptr, size_t size) {
        size_t old = out.size();
        out.resize(old + size);
        std::memcpy(out.data() + old, ptr, size);
    };

    // Write count (native endianness)
    append_raw(&count, sizeof(count));

    // Build contiguous array of SerializedEdit and append
    if (count > 0) {
        std::vector<SerializedEdit> arr;
        arr.reserve(count);
        for (const CrossChunkEdit& e : data->edits) {
            SerializedEdit se;
            se.type = static_cast<uint8_t>(e.block.type);
            se.metadata = e.block.metadata;
            se.pos = e.packedPos; // native endianness
            arr.push_back(se);
        }
        append_raw(arr.data(), arr.size() * sizeof(SerializedEdit));
    }

    return out;
}

const RegionCoord& RegionFile::GetRegionCoord() const {
    return _coord;
}

const std::string& RegionFile::GetPath() const {
    return _path;
}

void RegionManager::AddRegion(RegionFile&& rf) {
    RegionCoord coord = rf.GetRegionCoord();  // Get coordinate BEFORE moving
    regionList.push_front(std::move(rf));
    regionFiles.emplace(coord, regionList.begin());  // Use saved coordinate

    // Evict least-recently-used region if over capacity
    if (regionList.size() > MAX_REGIONS) {
        auto lastIt = std::prev(regionList.end());   // iterator to last element
        RegionCoord lastCoord = lastIt->GetRegionCoord();       // you need a way to get its key

        regionList.erase(lastIt);                    // remove from list
        regionFiles.erase(lastCoord);                // remove from map
    }
}

void RegionManager::RemoveRegion(const RegionCoord& coord) {
    auto it = regionFiles.find(coord);
    if (it == regionFiles.end()) return;
    regionList.erase(it->second);        // destroys RegionFile
    regionFiles.erase(it);               // remove map entry
}

RegionFile& RegionManager::AccessRegion(const RegionCoord& coord) {
    auto it = regionFiles.find(coord);
    if (it == regionFiles.end()) {
        throw std::runtime_error("Region not loaded");
    }

    // Move this region to the front (MRU)
    regionList.splice(regionList.begin(), regionList, it->second);

    // Update iterator stored in the map (it now points to begin())
    it->second = regionList.begin();

    return *(it->second);
}

bool RegionManager::FilePresent(const RegionCoord& coord) {
    auto it = regionFiles.find(coord);
    if (it == regionFiles.end()) {
        return false;
    }
    return true;
}