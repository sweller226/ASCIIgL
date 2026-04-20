#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>

#include <ASCIICraft/world/block/models/CubeModelBuilder.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>

namespace blockmodels {

namespace {
    static std::mutex g_modelLibraryMutex;

    inline void HashCombine(std::size_t& seed, std::size_t v) {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    inline std::size_t HashBytes(const std::vector<std::byte>& v) {
        std::size_t h = v.size();
        for (const std::byte b : v)
            h = h * 1099511628211ULL ^ static_cast<unsigned char>(std::to_integer<unsigned char>(b));
        return h;
    }

    inline std::size_t HashInts(const std::vector<int>& v) {
        std::size_t h = v.size();
        for (int x : v)
            HashCombine(h, static_cast<std::size_t>(x));
        return h;
    }

    inline std::size_t HashFaces(const std::vector<blockstate::FaceRange>& faces) {
        std::size_t h = faces.size();
        for (const auto& f : faces) {
            HashCombine(h, static_cast<std::size_t>(static_cast<unsigned>(f.cardinalFace)));
            HashCombine(h, static_cast<std::size_t>(f.vertByteOffset));
            HashCombine(h, static_cast<std::size_t>(f.vertByteCount));
            HashCombine(h, static_cast<std::size_t>(f.idxOffset));
            HashCombine(h, static_cast<std::size_t>(f.idxCount));
        }
        return h;
    }

    inline std::size_t HashLayer(const blockstate::RenderLayer& layer) {
        std::size_t h = 0;
        HashCombine(h, HashBytes(layer.vertices));
        HashCombine(h, HashInts(layer.indices));
        HashCombine(h, HashFaces(layer.faces));
        return h;
    }

    inline std::size_t FingerprintModel(const blockstate::BlockModel& m) {
        std::size_t h = 0;
        HashCombine(h, static_cast<std::size_t>(m.isFullBlock ? 1 : 0));
        HashCombine(h, static_cast<std::size_t>(m.opaqueNoCull ? 1 : 0));
        HashCombine(h, HashLayer(m.opaque));
        HashCombine(h, HashLayer(m.transparent));
        return h;
    }

    inline bool LayerEqual(const blockstate::RenderLayer& a, const blockstate::RenderLayer& b) {
        return a.vertices == b.vertices && a.indices == b.indices && a.faces == b.faces;
    }

    inline bool ModelsEqual(const blockstate::BlockModel& a, const blockstate::BlockModel& b) {
        return a.isFullBlock == b.isFullBlock &&
               a.opaqueNoCull == b.opaqueNoCull &&
               LayerEqual(a.opaque, b.opaque) &&
               LayerEqual(a.transparent, b.transparent);
    }

    inline uint32_t MixCoordHash(const uint32_t stateId, const int worldX, const int worldY, const int worldZ) {
        // Deterministic integer hash (FNV-1a style over four 32-bit lanes).
        uint32_t h = 2166136261u;
        auto mix = [&](uint32_t v) {
            h ^= v;
            h *= 16777619u;
        };
        mix(stateId);
        mix(static_cast<uint32_t>(worldX));
        mix(static_cast<uint32_t>(worldY));
        mix(static_cast<uint32_t>(worldZ));
        return h;
    }
} // namespace

// threadsafe for reading
const blockstate::BlockModel* BlockModelLibrary::GetModel(uint32_t stateId) const {
    if (stateId >= stateModelSets_.size()) return nullptr;
    const std::vector<ModelPtr>& set = stateModelSets_[stateId];
    if (set.empty()) return nullptr;
    return set.front().get();
}

const blockstate::BlockModel* BlockModelLibrary::GetModelForBlock(
    const uint32_t stateId,
    const int worldX,
    const int worldY,
    const int worldZ
) const {
    if (stateId >= stateModelSets_.size()) return nullptr;
    const std::vector<ModelPtr>& set = stateModelSets_[stateId];
    if (set.empty()) return nullptr;
    if (set.size() == 1) return set.front().get();

    const uint32_t h = MixCoordHash(stateId, worldX, worldY, worldZ);
    const size_t pick = static_cast<size_t>(h % static_cast<uint32_t>(set.size()));
    return set[pick].get();
}

void BlockModelLibrary::RegisterModel(uint32_t stateId, ModelPtr model, blockstate::BlockStateRegistry& bsr) {
    RegisterModelSet(stateId, { std::move(model) }, bsr);
}

void BlockModelLibrary::RegisterModel(uint16_t typeId, ModelPtr model, blockstate::BlockStateRegistry& bsr) {
    RegisterModelSet(typeId, { std::move(model) }, bsr);
}

void BlockModelLibrary::RegisterModelSet(
    const uint32_t stateId,
    const std::vector<ModelPtr>& models,
    blockstate::BlockStateRegistry& bsr
) {
    std::lock_guard<std::mutex> lock(g_modelLibraryMutex);

    if (stateId >= stateModelSets_.size()) {
        stateModelSets_.resize(stateId + 1);
    }

    std::vector<ModelPtr> newSet;
    newSet.reserve(models.size());

    bool fullBlockValue = false;
    bool fullBlockSet = false;

    for (const ModelPtr& inModel : models) {
        if (!inModel) {
            continue;
        }

        ModelPtr canonical = inModel;
        const std::size_t fp = FingerprintModel(*inModel);
        auto& bucket = modelsByFingerprint_[fp];
        for (const auto& candidate : bucket) {
            if (candidate && ModelsEqual(*candidate, *inModel)) {
                canonical = candidate;
                break;
            }
        }
        if (canonical == inModel) {
            bucket.push_back(canonical);
        }

        newSet.push_back(std::move(canonical));

        if (!fullBlockSet) {
            fullBlockValue = inModel->isFullBlock;
            fullBlockSet = true;
        }
    }

    stateModelSets_[stateId] = std::move(newSet);

    bsr.GetStateMutable(stateId).isFullBlock = fullBlockSet ? fullBlockValue : false;
}

void BlockModelLibrary::RegisterModelSet(
    const uint16_t typeId,
    const std::vector<ModelPtr>& models,
    blockstate::BlockStateRegistry& bsr
) {
    const blockstate::BlockType& type = bsr.GetType(typeId);
    const uint32_t base = type.baseStateId;
    const uint32_t count = type.stateCount;

    // Register the same model set for every state in this type.
    for (uint32_t i = 0; i < count; ++i) {
        RegisterModelSet(base + i, models, bsr);
    }
}

void BlockModelLibrary::RegisterCubeModel(uint16_t typeId, const CubeSpec& spec, blockstate::BlockStateRegistry& bsr) {
    auto model = std::make_shared<const blockstate::BlockModel>(BuildCubeModel(spec));
    RegisterModel(typeId, std::move(model), bsr);
}

} // namespace blockmodels

