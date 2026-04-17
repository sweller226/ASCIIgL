#include <ASCIICraft/world/blockmodels/BlockModelResolver.hpp>

#include <unordered_set>
#include <vector>

namespace blockmodels {

namespace {

    constexpr int kMaxParentDepth = 64;
    constexpr int kMaxTextureChain = 64;

    bool StartsWithHash(const std::string& s) {
        return !s.empty() && s[0] == '#';
    }

    std::string StripHashKey(const std::string& s) {
        return s.size() > 1 ? s.substr(1) : std::string();
    }

    jsonutil::LoadResult<std::string> ResolveTextureValue(
        const std::unordered_map<std::string, std::string>& textures,
        const std::string& value,
        const std::string& debugContext,
        std::unordered_set<std::string>& visiting,
        int depth
    ) {
        if (depth > kMaxTextureChain) {
            return jsonutil::Fail<std::string>(debugContext + ": texture variable chain too deep");
        }
        if (!StartsWithHash(value)) {
            return jsonutil::LoadResult<std::string>::Success(value);
        }
        const std::string key = StripHashKey(value);
        if (key.empty()) {
            return jsonutil::Fail<std::string>(debugContext + ": empty texture variable");
        }
        if (visiting.count(key)) {
            return jsonutil::Fail<std::string>(debugContext + ": texture variable cycle involving '#" + key + "'");
        }
        auto it = textures.find(key);
        if (it == textures.end()) {
            return jsonutil::Fail<std::string>(debugContext + ": missing texture variable '#" + key + "'");
        }
        visiting.insert(key);
        auto out = ResolveTextureValue(textures, it->second, debugContext, visiting, depth + 1);
        visiting.erase(key);
        return out;
    }

    jsonutil::LoadResult<std::string> ResolveTextureValue(
        const std::unordered_map<std::string, std::string>& textures,
        const std::string& value,
        const std::string& debugContext
    ) {
        std::unordered_set<std::string> visiting;
        return ResolveTextureValue(textures, value, debugContext, visiting, 0);
    }

    jsonutil::LoadResult<ResolvedBlockModelElement> ResolveElement(
        const BlockModelElementDef& elem,
        const std::unordered_map<std::string, std::string>& mergedTextures,
        const std::string& debugContext
    ) {
        ResolvedBlockModelElement out;
        out.from = elem.from;
        out.to = elem.to;
        out.shade = elem.shade;
        out.rotation = elem.rotation;
        for (const auto& kv : elem.faces) {
            const std::string& dir = kv.first;
            const auto& face = kv.second;
            auto texRes = ResolveTextureValue(mergedTextures, face.texture, debugContext + " face '" + dir + "'");
            if (!texRes.Ok()) return jsonutil::Fail<ResolvedBlockModelElement>(texRes.error.value_or("unknown error"));
            ResolvedBlockModelFace rf;
            rf.uv = face.uv;
            rf.texture = std::move(texRes.value.value());
            rf.cullface = face.cullface;
            rf.rotation = face.rotation;
            rf.tintindex = face.tintindex;
            out.faces[dir] = std::move(rf);
        }
        return jsonutil::LoadResult<ResolvedBlockModelElement>::Success(std::move(out));
    }

} // namespace

jsonutil::LoadResult<ResolvedBlockModelDefinition> ResolveBlockModel(JsonModelLoader& loader, const std::string& resourceId) {
    std::string rlError;
    auto modelId = ResourceLocation::Parse(resourceId, &rlError);
    if (!modelId.has_value()) {
        return jsonutil::Fail<ResolvedBlockModelDefinition>(rlError);
    }

    const std::string rootKey = modelId->ToString();
    std::vector<BlockModelDefinition> chain;
    std::unordered_set<std::string> parentVisited;
    ResourceLocation current = modelId.value();
    bool hasCubeAllInParentChain = false;

    for (int depth = 0; depth < kMaxParentDepth; ++depth) {
        const std::string key = current.ToString();
        if (current.path == "block/cube_all") {
            hasCubeAllInParentChain = true;
        }
        if (parentVisited.count(key)) {
            return jsonutil::Fail<ResolvedBlockModelDefinition>("parent chain cycle at '" + key + "'");
        }
        parentVisited.insert(key);

        auto loaded = loader.GetOrLoadBlockModel(key);
        if (!loaded.Ok()) {
            return jsonutil::Fail<ResolvedBlockModelDefinition>(loaded.error.value_or("unknown error"));
        }
        chain.push_back(std::move(loaded.value.value()));

        if (!chain.back().parent.has_value()) {
            break;
        }
        current = chain.back().parent.value();
    }

    if (chain.size() > static_cast<size_t>(kMaxParentDepth)) {
        return jsonutil::Fail<ResolvedBlockModelDefinition>("parent chain too deep starting at '" + rootKey + "'");
    }

    // Merge textures: ancestor first, leaf last — later overwrites (child wins).
    std::unordered_map<std::string, std::string> mergedTextures;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        for (const auto& kv : it->textures) {
            mergedTextures[kv.first] = kv.second;
        }
    }

    // ambientocclusion: root first, leaf last — last assignment wins (child overrides parent).
    std::optional<bool> mergedAo;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (it->ambientocclusion.has_value()) {
            mergedAo = it->ambientocclusion;
        }
    }

    // Elements: first model in chain (leaf first) with non-empty elements replaces inherited elements.
    const std::vector<BlockModelElementDef>* elementsSource = nullptr;
    for (const auto& m : chain) {
        if (!m.elements.empty()) {
            elementsSource = &m.elements;
            break;
        }
    }

    ResolvedBlockModelDefinition out;
    out.isFullBlock = hasCubeAllInParentChain;
    out.ambientocclusion = mergedAo;

    // Validate merged texture map can fully resolve # chains.
    for (const auto& kv : mergedTextures) {
        auto r = ResolveTextureValue(mergedTextures, kv.second, "textures['" + kv.first + "']");
        if (!r.Ok()) return jsonutil::Fail<ResolvedBlockModelDefinition>(r.error.value_or("unknown error"));
    }

    if (!elementsSource) {
        return jsonutil::LoadResult<ResolvedBlockModelDefinition>::Success(std::move(out));
    }

    for (size_t e = 0; e < elementsSource->size(); ++e) {
        auto re = ResolveElement((*elementsSource)[e], mergedTextures, rootKey + " elements[" + std::to_string(e) + "]");
        if (!re.Ok()) return jsonutil::Fail<ResolvedBlockModelDefinition>(re.error.value_or("unknown error"));
        out.elements.push_back(std::move(re.value.value()));
    }

    return jsonutil::LoadResult<ResolvedBlockModelDefinition>::Success(std::move(out));
}

} // namespace blockmodels
