#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <ASCIICraft/util/JsonUtil.hpp>

namespace blockmodels {

    struct ResourceLocation {
        std::string ns = "minecraft";
        std::string path;

        std::string ToString() const;
        static std::optional<ResourceLocation> Parse(const std::string& input, std::string* outError = nullptr);
    };

    struct BlockModelFaceDef {
        std::optional<std::array<float, 4>> uv;
        std::string texture;
        std::optional<std::string> cullface;
        int rotation = 0;
        std::optional<int> tintindex;
    };

    struct BlockModelElementRotationDef {
        std::array<float, 3> origin = { 0.0f, 0.0f, 0.0f };
        char axis = 'y';
        float angle = 0.0f;
        bool rescale = false;
    };

    struct BlockModelElementDef {
        std::array<float, 3> from = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> to = { 16.0f, 16.0f, 16.0f };
        std::optional<BlockModelElementRotationDef> rotation;
        std::optional<bool> shade;
        std::unordered_map<std::string, BlockModelFaceDef> faces;
    };

    struct BlockModelDefinition {
        std::optional<ResourceLocation> parent;
        std::unordered_map<std::string, std::string> textures;
        std::vector<BlockModelElementDef> elements;
        std::optional<bool> ambientocclusion;
    };

    class JsonModelLoader {
    public:
        explicit JsonModelLoader(std::string assetsRootPath);

        jsonutil::LoadResult<BlockModelDefinition> GetOrLoadBlockModel(const std::string& resourceId);
        void ClearCaches();

    private:
        std::string assetsRootPath_;

        std::unordered_map<std::string, BlockModelDefinition> modelByResource_;

        static std::string CanonicalResourceKey(const ResourceLocation& rl);

        std::string ResolveBlockModelPath(const ResourceLocation& rl) const;

        jsonutil::LoadResult<BlockModelDefinition> ParseBlockModelJsonText(const std::string& jsonText, const std::string& debugName) const;
    };

} // namespace blockmodels

