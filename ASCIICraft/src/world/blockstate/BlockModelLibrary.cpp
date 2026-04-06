#include <ASCIICraft/world/blockstate/BlockModelLibrary.hpp>

#include <ASCIICraft/world/blockstate/CubeModelBuilder.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

#include <cstddef>
#include <mutex>
#include <utility>

namespace blockstate {

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

    inline std::size_t HashLayer(const RenderLayer& layer) {
        std::size_t h = 0;
        HashCombine(h, HashBytes(layer.vertices));
        HashCombine(h, HashInts(layer.indices));
        return h;
    }

    inline std::size_t FingerprintModel(const BlockModel& m) {
        std::size_t h = 0;
        HashCombine(h, HashLayer(m.opaque));
        HashCombine(h, HashLayer(m.transparent));
        return h;
    }

    inline bool LayerEqual(const RenderLayer& a, const RenderLayer& b) {
        return a.vertices == b.vertices && a.indices == b.indices;
    }

    inline bool ModelsEqual(const BlockModel& a, const BlockModel& b) {
        return LayerEqual(a.opaque, b.opaque) &&
               LayerEqual(a.transparent, b.transparent);
    }
} // namespace

// threadsafe for reading
const BlockModel* BlockModelLibrary::GetModel(uint32_t stateId) const {
    if (stateId >= modelsByState_.size()) return nullptr;
    return modelsByState_[stateId].get();
}

void BlockModelLibrary::RegisterModel(uint32_t stateId, std::shared_ptr<const BlockModel> model, BlockStateRegistry& bsr) {
    std::lock_guard<std::mutex> lock(g_modelLibraryMutex);

    if (stateId >= modelsByState_.size())
        modelsByState_.resize(stateId + 1);

    bsr.GetStateMutable(stateId).isFullBlock = model ? model->isFullBlock : false;

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

void BlockModelLibrary::RegisterModel(uint16_t typeId, std::shared_ptr<const BlockModel> model, BlockStateRegistry& bsr) {
    const BlockType& type = bsr.GetType(typeId);
    const uint32_t base = type.baseStateId;
    const uint32_t count = type.stateCount;

    // Register the same shared model pointer for every state in this type.
    for (uint32_t i = 0; i < count; ++i) {
        RegisterModel(base + i, model, bsr);
    }
}

void BlockModelLibrary::RegisterCubeModel(uint16_t typeId, const CubeSpec& spec, BlockStateRegistry& bsr) {
    auto model = std::make_shared<const BlockModel>(BuildCubeModel(spec));
    RegisterModel(typeId, std::move(model), bsr);
}

} // namespace blockstate

