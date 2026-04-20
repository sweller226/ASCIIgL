#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>

#include <utility>

#include <nlohmann/json.hpp>

namespace blockmodels {

namespace {

    using json = nlohmann::json;

    bool IsValidToken(const std::string& s) {
        if (s.empty()) return false;
        if (s.find("..") != std::string::npos) return false;
        if (s.find('\\') != std::string::npos) return false;
        return true;
    }

    template <typename T>
    bool Unwrap(jsonutil::LoadResult<T>&& in, T& out, std::string& err) {
        if (!in.Ok()) {
            err = in.error.value_or("unknown error");
            return false;
        }
        out = std::move(in.value.value());
        return true;
    }

} // namespace

std::string ResourceLocation::ToString() const {
    return ns + ":" + path;
}

std::optional<ResourceLocation> ResourceLocation::Parse(const std::string& input, std::string* outError) {
    ResourceLocation out;

    const size_t colon = input.find(':');
    if (colon == std::string::npos) {
        out.path = input;
    } else {
        out.ns = input.substr(0, colon);
        out.path = input.substr(colon + 1);
    }

    if (!IsValidToken(out.ns) || !IsValidToken(out.path)) {
        if (outError) *outError = "invalid resource location '" + input + "'";
        return std::nullopt;
    }

    return out;
}

JsonModelLoader::JsonModelLoader(std::string assetsRootPath)
    : assetsRootPath_(std::move(assetsRootPath)) {}

jsonutil::LoadResult<BlockModelDefinition> JsonModelLoader::GetOrLoadBlockModel(const std::string& resourceId) {
    std::string rlError;
    auto rl = ResourceLocation::Parse(resourceId, &rlError);
    if (!rl.has_value()) {
        return jsonutil::LoadResult<BlockModelDefinition>::Failure(rlError);
    }

    const std::string key = CanonicalResourceKey(rl.value());
    auto cached = modelByResource_.find(key);
    if (cached != modelByResource_.end()) {
        return jsonutil::LoadResult<BlockModelDefinition>::Success(cached->second);
    }

    const std::string path = ResolveBlockModelPath(rl.value());
    auto textRes = jsonutil::ReadFileText(path);
    if (!textRes.Ok()) {
        return jsonutil::LoadResult<BlockModelDefinition>::Failure(textRes.error.value_or("unknown error"));
    }

    auto parsedRes = ParseBlockModelJsonText(textRes.value.value(), key);
    if (!parsedRes.Ok()) {
        return parsedRes;
    }

    modelByResource_[key] = parsedRes.value.value();
    return jsonutil::LoadResult<BlockModelDefinition>::Success(modelByResource_[key]);
}

void JsonModelLoader::ClearCaches() {
    modelByResource_.clear();
}

std::string JsonModelLoader::CanonicalResourceKey(const ResourceLocation& rl) {
    return rl.ToString();
}

std::string JsonModelLoader::ResolveBlockModelPath(const ResourceLocation& rl) const {
    return assetsRootPath_ + "/assets/" + rl.ns + "/models/" + rl.path + ".json";
}

jsonutil::LoadResult<BlockModelDefinition> JsonModelLoader::ParseBlockModelJsonText(const std::string& jsonText, const std::string& debugName) const {
    auto parsed = jsonutil::ParseJson(jsonText, debugName);
    if (!parsed.Ok()) return jsonutil::Fail<BlockModelDefinition>(parsed.error.value_or("unknown error"));
    const json& root = parsed.value.value();
    if (!root.is_object()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": root must be an object");

    BlockModelDefinition out;

    if (root.contains("parent")) {
        if (!root["parent"].is_string()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": 'parent' must be a string");
        std::string rlError;
        auto parent = ResourceLocation::Parse(root["parent"].get<std::string>(), &rlError);
        if (!parent.has_value()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": invalid parent: " + rlError);
        out.parent = parent.value();
    }

    if (root.contains("textures")) {
        if (!root["textures"].is_object()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": 'textures' must be an object");
        for (auto it = root["textures"].begin(); it != root["textures"].end(); ++it) {
            if (!it.value().is_string()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": texture value for '" + it.key() + "' must be string");
            out.textures[it.key()] = it.value().get<std::string>();
        }
    }

    if (root.contains("elements")) {
        if (!root["elements"].is_array()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": 'elements' must be an array");

        for (size_t e = 0; e < root["elements"].size(); ++e) {
            const json& je = root["elements"][e];
            if (!je.is_object()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "] must be an object");

            BlockModelElementDef elem;
            if (!je.contains("from") || !je.contains("to")) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "] requires 'from' and 'to'");
            std::string err;
            if (!Unwrap(jsonutil::ParseFloatArray<3>(je["from"], debugName, "elements[" + std::to_string(e) + "].from"), elem.from, err)) return jsonutil::Fail<BlockModelDefinition>(err);
            if (!Unwrap(jsonutil::ParseFloatArray<3>(je["to"], debugName, "elements[" + std::to_string(e) + "].to"), elem.to, err)) return jsonutil::Fail<BlockModelDefinition>(err);

            if (je.contains("shade")) {
                if (!je["shade"].is_boolean()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].shade must be boolean");
                elem.shade = je["shade"].get<bool>();
            }

            if (je.contains("rotation")) {
                const auto& jr = je["rotation"];
                if (!jr.is_object()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].rotation must be object");
                BlockModelElementRotationDef r;
                if (!jr.contains("origin") || !jr.contains("axis") || !jr.contains("angle")) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].rotation requires origin/axis/angle");
                std::string err;
                if (!Unwrap(jsonutil::ParseFloatArray<3>(jr["origin"], debugName, "elements[" + std::to_string(e) + "].rotation.origin"), r.origin, err)) return jsonutil::Fail<BlockModelDefinition>(err);
                if (!jr["axis"].is_string() || jr["axis"].get<std::string>().size() != 1) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].rotation.axis must be string x|y|z");
                r.axis = jr["axis"].get<std::string>()[0];
                if (r.axis != 'x' && r.axis != 'y' && r.axis != 'z') return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].rotation.axis must be x|y|z");
                if (!jr["angle"].is_number()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].rotation.angle must be numeric");
                r.angle = jr["angle"].get<float>();

                if (jr.contains("rescale")) {
                    if (!jr["rescale"].is_boolean()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].rotation.rescale must be boolean");
                    r.rescale = jr["rescale"].get<bool>();
                }
                elem.rotation = r;
            }

            if (je.contains("faces")) {
                const auto& jf = je["faces"];
                if (!jf.is_object()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": elements[" + std::to_string(e) + "].faces must be object");

                for (auto fit = jf.begin(); fit != jf.end(); ++fit) {
                    const std::string dir = fit.key();
                    const json& faceJson = fit.value();
                    if (!faceJson.is_object()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": face '" + dir + "' must be object");
                    if (!faceJson.contains("texture") || !faceJson["texture"].is_string()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": face '" + dir + "' missing string texture");

                    BlockModelFaceDef face;
                    face.texture = faceJson["texture"].get<std::string>();
                    std::string err;

                    if (faceJson.contains("rotation")) {
                        if (!faceJson["rotation"].is_number_integer()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": face '" + dir + "' rotation must be integer");
                        face.rotation = faceJson["rotation"].get<int>();
                    }
                    if (faceJson.contains("cullface")) {
                        if (!faceJson["cullface"].is_string()) return jsonutil::Fail<BlockModelDefinition>(debugName + ": face '" + dir + "' cullface must be string");
                        face.cullface = faceJson["cullface"].get<std::string>();
                    }
                    if (faceJson.contains("uv")) {
                        std::array<float, 4> uv{};
                        if (!Unwrap(jsonutil::ParseFloatArray<4>(faceJson["uv"], debugName, "face '" + dir + "'.uv"), uv, err)) return jsonutil::Fail<BlockModelDefinition>(err);
                        face.uv = std::move(uv);
                    }

                    elem.faces[dir] = std::move(face);
                }
            }

            out.elements.push_back(std::move(elem));
        }
    }

    return jsonutil::LoadResult<BlockModelDefinition>::Success(out);
}

} // namespace blockmodels

