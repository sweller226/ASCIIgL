#include <ASCIICraft/world/ChunkRegion.hpp>

#include <filesystem>
#include <algorithm>
#include <cassert>
#include <limits>

RegionFile::RegionFile(const RegionCoord& coord) : _coord(coord) {
    // Ensure chunkIndexes vector is sized before any reads/writes
    chunkIndexes.resize(static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE);

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

    // Validate local coords
    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        return false;
    }

    uint32_t off = indexOffset(lp);
    if (off >= chunkIndexes.size()) return false;

    auto& entry = chunkIndexes[off];
    if (!(entry.flags & 0x1)) return false;

    if (entry.length == 0) return false;

    std::vector<uint8_t> blob;
    blob.resize(entry.length);

    _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);
    _file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!_file.good()) throw std::runtime_error("Failed to seek region _file for read");

    _file.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
    if (_file.gcount() != static_cast<std::streamsize>(blob.size())) throw std::runtime_error("Failed to read full chunk blob");

    parseChunkBlob(blob, out);

    _file.close();

    return true;
}

bool RegionFile::SaveChunk(const Chunk* data) {
    RegionCoord rp = data->GetCoord().ToRegionCoord();
    glm::ivec3 lp = data->GetCoord().ToLocalRegion(rp);

    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        throw std::out_of_range("Local chunk coords out of region bounds");
    }

    uint32_t off = indexOffset(lp);
    if (off >= chunkIndexes.size()) throw std::out_of_range("Local chunk coords out of region bounds");

    auto& entry = chunkIndexes[off];

    std::vector<uint8_t> raw = buildChunkBlob(data);

    _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);
    _file.seekp(0, std::ios::end);
    if (!_file.good()) throw std::runtime_error("Failed to seek region _file for write");
    std::streampos p = _file.tellp();
    if (p == -1) throw std::runtime_error("tellp failed");
    uint32_t offset = static_cast<uint32_t>(p);

    _file.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    _file.flush();

    if (!(entry.flags & 0x1)) header.chunkCount++;

    entry.offset = offset;
    entry.length = static_cast<uint32_t>(raw.size());
    entry.flags = static_cast<uint8_t>(entry.flags | 0x1);

    writeHeaderAndIndex();

    _file.close();

    return true;
}

bool RegionFile::LoadMetaData(const ChunkCoord& pos, MetaBucket* out) {
    RegionCoord rp = pos.ToRegionCoord();
    glm::ivec3 lp = pos.ToLocalRegion(rp);

    // Validate local coords
    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        return false;
    }

    uint32_t off = indexOffset(lp);
    if (off >= metaIndexes.size()) return false;

    auto& entry = metaIndexes[off];
    if (!(entry.flags & 0x1)) return false; // not present
    if (entry.length == 0) return false;

    // Ensure metaStart is sane
    uint32_t metaBase = header.metaStart;
    if (metaBase == 0) {
        // fallback: compute expected metaStart if header wasn't initialized
        const size_t entryCount = static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE;
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        metaBase = header.chunkStart ? header.metaStart : (headerSize + chunkIndexTableSize + metaIndexTableSize);
    }

    _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);

    _file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!_file.good()) throw std::runtime_error("Failed to seek region _file for read");

    std::vector<uint8_t> blob;
    blob.resize(entry.length);
    _file.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
    if (_file.gcount() != static_cast<std::streamsize>(blob.size()))
        throw std::runtime_error("Failed to read full meta blob");

    // Parse into MetaBucket (implement parseMetaBlob to match buildMetaBlob)
    parseMetaBlob(blob, out);

    _file.close();

    return true;
}

bool RegionFile::SaveMetaData(const ChunkCoord& pos, const MetaBucket* data) {
    RegionCoord rp = pos.ToRegionCoord();
    glm::ivec3 lp = pos.ToLocalRegion(rp);

    if (lp.x < 0 || lp.y < 0 || lp.z < 0 ||
        lp.x >= REGION_SIZE || lp.y >= REGION_SIZE || lp.z >= REGION_SIZE) {
        throw std::out_of_range("Local chunk coords out of region bounds");
    }

    uint32_t off = indexOffset(lp);
    if (off >= metaIndexes.size()) throw std::out_of_range("Local chunk coords out of region bounds");

    auto& entry = metaIndexes[off];

    // Build serialized meta blob
    std::vector<uint8_t> raw = buildMetaBlob(data);
    uint32_t newLen = static_cast<uint32_t>(raw.size());

    // Ensure header.metaStart is initialized
    if (header.metaStart == 0) {
        // If chunkStart is set, use it; otherwise compute default layout
        const size_t entryCount = static_cast<size_t>(REGION_SIZE) * REGION_SIZE * REGION_SIZE;
        const uint32_t headerSize = static_cast<uint32_t>(sizeof(RegionHeader));
        const uint32_t chunkIndexTableSize = static_cast<uint32_t>(entryCount * sizeof(ChunkIndexEntry));
        const uint32_t metaIndexTableSize  = static_cast<uint32_t>(entryCount * sizeof(MetaBucketIndexEntry));
        header.chunkStart = header.chunkStart ? header.chunkStart : (headerSize + chunkIndexTableSize + metaIndexTableSize);
        header.metaStart = header.chunkStart;
    }

    // Append blob to EOF
    _file.open(_path, std::ios::binary | std::ios::in | std::ios::out);
    _file.seekp(0, std::ios::end);
    if (!_file.good()) throw std::runtime_error("Failed to seek region _file for write");
    std::streampos p = _file.tellp();
    if (p == -1) throw std::runtime_error("tellp failed");
    uint64_t offset = static_cast<uint64_t>(p);

    _file.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    _file.flush();

    // Update meta index entry
    // packedCoord: store linear index for now; replace with a packing helper if desired
    entry.packedCoord = off;
    entry.offset = offset;
    entry.length = newLen;
    entry.flags = static_cast<uint8_t>(entry.flags | 0x1);

    // Persist header + both index tables
    writeHeaderAndIndex();

    _file.close();

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
    regionList.push_front(std::move(rf));
    regionFiles.emplace(rf.GetRegionCoord(), regionList.begin());

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