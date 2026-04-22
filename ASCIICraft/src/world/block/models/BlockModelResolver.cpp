#include <ASCIICraft/world/block/models/BlockModelResolver.hpp>

#include <cmath>
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

    bool NearlyEq(const float a, const float b, const float eps = 0.0001f) {
        return std::abs(a - b) <= eps;
    }

    bool IsAxisAlignedFullCubeElement(const BlockModelElementDef& e) {
        return NearlyEq(e.from[0], 0.0f) && NearlyEq(e.from[1], 0.0f) && NearlyEq(e.from[2], 0.0f) &&
               NearlyEq(e.to[0], 16.0f) && NearlyEq(e.to[1], 16.0f) && NearlyEq(e.to[2], 16.0f);
    }

    bool HasAllSixFaces(const BlockModelElementDef& e) {
        return e.faces.find("up") != e.faces.end() &&
               e.faces.find("down") != e.faces.end() &&
               e.faces.find("north") != e.faces.end() &&
               e.faces.find("south") != e.faces.end() &&
               e.faces.find("east") != e.faces.end() &&
               e.faces.find("west") != e.faces.end();
    }

    bool IsGeometryFullBlock(const std::vector<BlockModelElementDef>* elementsSource) {
        if (elementsSource == nullptr || elementsSource->empty()) {
            return false;
        }
        // Some vanilla models (e.g. grass) add an overlay element on top of a full cube.
        // For neighbor occlusion, treat them as full-block if any element is a full
        // axis-aligned cube with all six faces and no element rotation.
        for (const BlockModelElementDef& e : *elementsSource) {
            if (e.rotation.has_value()) {
                continue;
            }
            if (IsAxisAlignedFullCubeElement(e) && HasAllSixFaces(e)) {
                return true;
            }
        }
        return false;
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
    bool hasCubeColumnInParentChain = false;
    bool hasCrossParentInChain = false;
    bool hasTorchParentInChain = false;

    for (int depth = 0; depth < kMaxParentDepth; ++depth) {
        const std::string key = current.ToString();
        if (current.path == "block/cube_all") {
            hasCubeAllInParentChain = true;
        }
        if (current.path == "block/cube_column") {
            hasCubeColumnInParentChain = true;
        }
        if (current.path == "block/cross") {
            hasCrossParentInChain = true;
        }
        if (current.path == "block/torch" || current.path == "block/torch_wall") {
            hasTorchParentInChain = true;
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

    // Merge textures: ancestor first, leaf last â€” later overwrites (child wins).
    std::unordered_map<std::string, std::string> mergedTextures;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        for (const auto& kv : it->textures) {
            mergedTextures[kv.first] = kv.second;
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
    out.isFullBlock = hasCubeAllInParentChain || hasCubeColumnInParentChain || IsGeometryFullBlock(elementsSource);
    // Cross and torch-like quads should render in the opaque-no-cull bucket.
    out.opaqueNoCull = hasCrossParentInChain || hasTorchParentInChain;

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
