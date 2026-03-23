#include <ASCIICraft/world/blockstate/BlockModelLibrary.hpp>

#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <cstddef>
#include <mutex>
#include <utility>

namespace blockstate {

namespace {
    static std::mutex g_modelLibraryMutex;

    // Content-dedup hash over the generated mesh buffers.
    inline void HashCombine(std::size_t& seed, std::size_t v) {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    inline std::size_t HashBytes(const std::vector<std::byte>& v) {
        std::size_t h = v.size();
        for (const std::byte b : v) {
            h = h * 1099511628211ULL ^ static_cast<unsigned char>(std::to_integer<unsigned char>(b));
        }
        return h;
    }

    inline std::size_t HashInts(const std::vector<int>& v) {
        std::size_t h = v.size();
        for (int x : v) {
            HashCombine(h, static_cast<std::size_t>(x));
        }
        return h;
    }

    inline std::size_t FingerprintModel(const BlockModel& m) {
        std::size_t h = 0;
        HashCombine(h, HashBytes(m.opaqueVertices));
        HashCombine(h, HashInts(m.opaqueIndices));
        HashCombine(h, HashBytes(m.transparentVertices));
        HashCombine(h, HashInts(m.transparentIndices));
        return h;
    }

    inline bool ModelsEqual(const BlockModel& a, const BlockModel& b) {
        return a.opaqueVertices == b.opaqueVertices &&
               a.opaqueIndices == b.opaqueIndices &&
               a.transparentVertices == b.transparentVertices &&
               a.transparentIndices == b.transparentIndices;
    }
} // namespace

BlockModelLibrary::ModelPtr BlockModelLibrary::GetModel(uint32_t stateId, const BlockStateRegistry& bsr) {
    (void)bsr; // Lookup-only; models must be registered explicitly.
    std::lock_guard<std::mutex> lock(g_modelLibraryMutex);

    // Fast path: already resolved for this exact stateId.
    if (auto it = modelsByState_.find(stateId); it != modelsByState_.end()) {
        return it->second;
    }

    // If it wasn't registered, return nullptr (no auto-build).
    return nullptr;
}

void BlockModelLibrary::RegisterModel(uint32_t stateId, std::shared_ptr<const BlockModel> model) {
    std::lock_guard<std::mutex> lock(g_modelLibraryMutex);
    if (!model) {
        modelsByState_[stateId] = nullptr;
        return;
    }

    const std::size_t fp = FingerprintModel(*model);
    auto& bucket = modelsByFingerprint_[fp];
    for (const auto& candidate : bucket) {
        if (candidate && ModelsEqual(*candidate, *model)) {
            modelsByState_[stateId] = candidate;
            return;
        }
    }

    bucket.push_back(model);
    modelsByState_[stateId] = std::move(model);
}

} // namespace blockstate

