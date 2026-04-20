#include <ASCIICraft/world/block/state/JsonBlockStateLoader.hpp>

#include <utility>

#include <nlohmann/json.hpp>

namespace blockstate {

namespace {

    using json = nlohmann::json;

    std::string NormalizeBlockstateModelId(const std::string& rawModelId) {
        const size_t colon = rawModelId.find(':');
        if (colon != std::string::npos) {
            const std::string ns = rawModelId.substr(0, colon);
            const std::string path = rawModelId.substr(colon + 1);
            if (path.find('/') == std::string::npos) {
                return ns + ":block/" + path;
            }
            return rawModelId;
        }

        if (rawModelId.find('/') == std::string::npos) {
            return "minecraft:block/" + rawModelId;
        }
        return "minecraft:" + rawModelId;
    }

    bool GetInt(const json& obj, const char* key, int& out, std::string& err, const std::string& debugName) {
        if (!obj.contains(key)) return true;
        if (!obj[key].is_number_integer()) {
            err = debugName + ": '" + key + "' must be an integer";
            return false;
        }
        out = obj[key].get<int>();
        return true;
    }

    bool GetBool(const json& obj, const char* key, bool& out, std::string& err, const std::string& debugName) {
        if (!obj.contains(key)) return true;
        if (!obj[key].is_boolean()) {
            err = debugName + ": '" + key + "' must be a boolean";
            return false;
        }
        out = obj[key].get<bool>();
        return true;
    }

    jsonutil::LoadResult<VariantModelRef> ParseVariantEntry(const json& j, const std::string& debugName) {
        if (!j.is_object()) return jsonutil::Fail<VariantModelRef>(debugName + ": variant entry must be an object");
        if (!j.contains("model") || !j["model"].is_string()) return jsonutil::Fail<VariantModelRef>(debugName + ": variant entry missing string 'model'");

        std::string rlError;
        const std::string normalizedModelId = NormalizeBlockstateModelId(j["model"].get<std::string>());
        auto modelRl = blockmodels::ResourceLocation::Parse(normalizedModelId, &rlError);
        if (!modelRl.has_value()) {
            return jsonutil::Fail<VariantModelRef>(debugName + ": invalid variant model resource location: " + rlError);
        }

        VariantModelRef out;
        out.model = modelRl.value();
        std::string err;
        if (!GetInt(j, "x", out.x, err, debugName) ||
            !GetInt(j, "y", out.y, err, debugName) ||
            !GetBool(j, "uvlock", out.uvlock, err, debugName)) {
            return jsonutil::Fail<VariantModelRef>(err);
        }
        return jsonutil::LoadResult<VariantModelRef>::Success(std::move(out));
    }

} // namespace

JsonBlockStateLoader::JsonBlockStateLoader(std::string assetsRootPath)
    : assetsRootPath_(std::move(assetsRootPath)) {}

std::string JsonBlockStateLoader::FormatBlockstatePath(const std::string& blockstateResourceId) const {
    std::string rlError;
    const auto rl = blockmodels::ResourceLocation::Parse(blockstateResourceId, &rlError);
    if (!rl.has_value()) {
        return assetsRootPath_ + "/assets/<invalid-blockstate-id>/" + blockstateResourceId;
    }
    return ResolveBlockstatePath(rl.value());
}

jsonutil::LoadResult<BlockstateDefinition> JsonBlockStateLoader::GetOrLoadBlockstate(const std::string& resourceId) {
    std::string rlError;
    auto rl = blockmodels::ResourceLocation::Parse(resourceId, &rlError);
    if (!rl.has_value()) {
        return jsonutil::LoadResult<BlockstateDefinition>::Failure(rlError);
    }

    const std::string key = CanonicalResourceKey(rl.value());
    auto cached = blockstateByResource_.find(key);
    if (cached != blockstateByResource_.end()) {
        return jsonutil::LoadResult<BlockstateDefinition>::Success(cached->second);
    }

    const std::string path = ResolveBlockstatePath(rl.value());
    auto textRes = jsonutil::ReadFileText(path);
    if (!textRes.Ok()) {
        return jsonutil::LoadResult<BlockstateDefinition>::Failure(textRes.error.value_or("unknown error"));
    }

    auto parsedRes = ParseBlockstateJsonText(textRes.value.value(), key);
    if (!parsedRes.Ok()) {
        return parsedRes;
    }

    blockstateByResource_[key] = parsedRes.value.value();
    return jsonutil::LoadResult<BlockstateDefinition>::Success(blockstateByResource_[key]);
}

void JsonBlockStateLoader::ClearCaches() {
    blockstateByResource_.clear();
}

std::string JsonBlockStateLoader::CanonicalResourceKey(const blockmodels::ResourceLocation& rl) {
    return rl.ToString();
}

std::string JsonBlockStateLoader::ResolveBlockstatePath(const blockmodels::ResourceLocation& rl) const {
    return assetsRootPath_ + "/assets/" + rl.ns + "/blockstates/" + rl.path + ".json";
}

jsonutil::LoadResult<BlockstateDefinition> JsonBlockStateLoader::ParseBlockstateJsonText(const std::string& jsonText, const std::string& debugName) const {
    auto parsed = jsonutil::ParseJson(jsonText, debugName);
    if (!parsed.Ok()) return jsonutil::Fail<BlockstateDefinition>(parsed.error.value_or("unknown error"));
    const json& root = parsed.value.value();
    if (!root.contains("variants") || !root["variants"].is_object()) return jsonutil::Fail<BlockstateDefinition>(debugName + ": missing object field 'variants'");
    const auto& variants = root["variants"];

    BlockstateDefinition def;
    for (auto it = variants.begin(); it != variants.end(); ++it) {
        const std::string variantKey = it.key();
        const json& variantValue = it.value();

        if (variantValue.is_object()) {
            auto one = ParseVariantEntry(variantValue, debugName + " variants['" + variantKey + "']");
            if (!one.Ok()) return jsonutil::Fail<BlockstateDefinition>(one.error.value_or("unknown error"));
            std::vector<VariantModelRef> refs;
            refs.push_back(std::move(one.value.value()));
            def.variants[variantKey] = std::move(refs);
        } else if (variantValue.is_array()) {
            if (variantValue.empty()) {
                return jsonutil::Fail<BlockstateDefinition>(debugName + ": variant '" + variantKey + "' array is empty");
            }
            std::vector<VariantModelRef> refs;
            refs.reserve(variantValue.size());
            for (size_t idx = 0; idx < variantValue.size(); ++idx) {
                auto one = ParseVariantEntry(
                    variantValue[idx],
                    debugName + " variants['" + variantKey + "'][" + std::to_string(idx) + "]"
                );
                if (!one.Ok()) return jsonutil::Fail<BlockstateDefinition>(one.error.value_or("unknown error"));
                refs.push_back(std::move(one.value.value()));
            }
            def.variants[variantKey] = std::move(refs);
        } else {
            return jsonutil::Fail<BlockstateDefinition>(debugName + ": variant '" + variantKey + "' must be object or array");
        }
    }
    return jsonutil::LoadResult<BlockstateDefinition>::Success(std::move(def));
}

} // namespace blockstate
