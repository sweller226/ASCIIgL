#include <ASCIICraft/world/block/state/VariantKey.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <cassert>

namespace blockstate {

std::string BuildVariantKey(const BlockStateRegistry& bsr, uint32_t stateId) {
    if (!bsr.IsValidState(stateId)) {
        return {};
    }

    const uint16_t typeId = bsr.GetTypeIdFromState(stateId);
    const BlockType& type = bsr.GetType(typeId);
    if (type.properties.empty()) {
        return {};
    }

    std::string out;
    out.reserve(type.properties.size() * 12u);
    for (size_t i = 0; i < type.properties.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        const BlockProperty& p = type.properties[i];
        out += p.name;
        out += '=';
        out += bsr.GetPropertyValue(stateId, p.name);
    }
    return out;
}

void AssertUniqueVariantKeysPerType(
    const BlockStateRegistry& bsr,
    const uint16_t typeId,
    const char* context
) {
    if (typeId >= bsr.GetTotalTypeCount()) {
        return;
    }

    const BlockType& type = bsr.GetType(typeId);
    if (type.stateCount <= 1) {
        return;
    }

    std::unordered_map<std::string, uint32_t> firstStateByKey;
    firstStateByKey.reserve(type.stateCount);

    for (uint32_t i = 0; i < type.stateCount; ++i) {
        const uint32_t stateId = type.baseStateId + i;
        const std::string k = BuildVariantKey(bsr, stateId);
        const auto inserted = firstStateByKey.emplace(k, stateId);
        if (!inserted.second) {
            const std::string ctx = context ? std::string(context) : std::string("AssertUniqueVariantKeysPerType");
            ASCIIgL::Logger::Error(
                ctx + ": duplicate variant key for type '" + type.name + "' key \"" + k + "\" stateIds " +
                std::to_string(inserted.first->second) + " and " + std::to_string(stateId)
            );
            assert(false && "AssertUniqueVariantKeysPerType: duplicate variant keys");
            return;
        }
    }
}

} // namespace blockstate
