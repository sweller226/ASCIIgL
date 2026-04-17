#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <ASCIICraft/util/JsonUtil.hpp>
#include <ASCIICraft/world/blockmodels/JsonModelLoader.hpp>

namespace blockstate {

    struct VariantModelRef {
        blockmodels::ResourceLocation model;
        int x = 0;
        int y = 0;
        bool uvlock = false;
        int weight = 1;
    };

    struct BlockstateDefinition {
        std::unordered_map<std::string, std::vector<VariantModelRef>> variants;
    };

    class JsonBlockStateLoader {
    public:
        explicit JsonBlockStateLoader(std::string assetsRootPath);

        jsonutil::LoadResult<BlockstateDefinition> GetOrLoadBlockstate(const std::string& resourceId);
        void ClearCaches();

    private:
        std::string assetsRootPath_;

        std::unordered_map<std::string, BlockstateDefinition> blockstateByResource_;

        static std::string CanonicalResourceKey(const blockmodels::ResourceLocation& rl);

        std::string ResolveBlockstatePath(const blockmodels::ResourceLocation& rl) const;

        jsonutil::LoadResult<BlockstateDefinition> ParseBlockstateJsonText(const std::string& jsonText, const std::string& debugName) const;
    };

} // namespace blockstate
