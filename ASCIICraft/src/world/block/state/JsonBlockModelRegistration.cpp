#include <ASCIICraft/world/block/state/JsonBlockModelRegistration.hpp>

#include <ASCIICraft/world/block/models/BlockModelBaker.hpp>
#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/models/BlockModelResolver.hpp>
#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/JsonBlockStateLoader.hpp>
#include <ASCIICraft/world/block/state/VariantKey.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <memory>

namespace blockstate {

namespace {

    const std::vector<VariantModelRef>* LookupVariantModelRefs(
        const BlockstateDefinition& def,
        const std::string& exactKey,
        const std::string& typeName,
        const std::string& blockstatePathHint
    ) {
        const auto at = [&](const std::string& k) -> const std::vector<VariantModelRef>* {
            const auto it = def.variants.find(k);
            if (it == def.variants.end()) {
                return nullptr;
            }
            return &it->second;
        };

        if (const std::vector<VariantModelRef>* p = at(exactKey)) {
            return p;
        }
        if (!exactKey.empty()) {
            if (const std::vector<VariantModelRef>* p = at("")) {
                return p;
            }
        }
        if (exactKey != "normal") {
            if (const std::vector<VariantModelRef>* p = at("normal")) {
                return p;
            }
        }

        ASCIIgL::Logger::Warning(
            "JsonBlockModelRegistration: no variant for type='" + typeName + "' builtKey=\"" + exactKey +
            "\" (tried fallbacks) path=" + blockstatePathHint
        );
        return nullptr;
    }

} // namespace

bool RegisterJsonBackedBlockType(
    const std::string& typeName,
    BlockStateRegistry& bsr,
    blockmodels::BlockModelLibrary& modelLibrary,
    JsonBlockStateLoader& blockstateLoader,
    blockmodels::JsonModelLoader& modelLoader
) {
    const uint16_t typeId = bsr.GetTypeId(typeName);
    if (bsr.GetType(typeId).name != typeName) {
        ASCIIgL::Logger::Error("JsonBlockModelRegistration: unknown block type '" + typeName + "'");
        return false;
    }

    const std::string pathHint = blockstateLoader.FormatBlockstatePath(typeName);
    auto blockstateRes = blockstateLoader.GetOrLoadBlockstate(typeName);
    if (!blockstateRes.Ok()) {
        ASCIIgL::Logger::Error(
            "JsonBlockModelRegistration: failed to load blockstate for '" + typeName + "': " +
            blockstateRes.error.value_or("unknown error") + " path=" + pathHint
        );
        return false;
    }

    const BlockstateDefinition& def = blockstateRes.value.value();
    const BlockType& type = bsr.GetType(typeId);

    bool allOk = true;

    for (uint32_t i = 0; i < type.stateCount; ++i) {
        const uint32_t stateId = type.baseStateId + i;
        const std::string variantKey = BuildVariantKey(bsr, stateId);
        const std::vector<VariantModelRef>* refs = LookupVariantModelRefs(def, variantKey, typeName, pathHint);
        if (refs == nullptr || refs->empty()) {
            allOk = false;
            continue;
        }

        std::vector<blockmodels::BlockModelLibrary::ModelPtr> modelSet;
        modelSet.reserve(refs->size());
        bool stateOk = true;

        for (size_t m = 0; m < refs->size(); ++m) {
            const VariantModelRef& ref = (*refs)[m];
            const std::string modelId = ref.model.ToString();

            auto resolved = blockmodels::ResolveBlockModel(modelLoader, modelId);
            if (!resolved.Ok()) {
                ASCIIgL::Logger::Error(
                    "JsonBlockModelRegistration: ResolveBlockModel failed type='" + typeName + "' stateId=" +
                    std::to_string(stateId) + " variantKey=\"" + variantKey + "\" model='" + modelId + "': " +
                    resolved.error.value_or("unknown error")
                );
                stateOk = false;
                break;
            }

            auto baked = blockmodels::BakeResolvedModel(
                resolved.value.value(),
                ref.x,
                ref.y,
                ref.uvlock
            );
            if (!baked.Ok()) {
                ASCIIgL::Logger::Error(
                    "JsonBlockModelRegistration: BakeResolvedModel failed type='" + typeName + "' stateId=" +
                    std::to_string(stateId) + " variantKey=\"" + variantKey + "\" model='" + modelId + "': " +
                    baked.error.value_or("unknown error")
                );
                stateOk = false;
                break;
            }

            modelSet.push_back(std::make_shared<const blockstate::BlockModel>(std::move(baked.value.value())));
        }

        if (!stateOk || modelSet.empty()) {
            allOk = false;
            continue;
        }

        modelLibrary.RegisterModelSet(stateId, modelSet, bsr);
    }

    return allOk;
}

} // namespace blockstate
