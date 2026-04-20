#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <ASCIICraft/util/JsonUtil.hpp>
#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>

namespace blockstate {

    struct VariantModelRef {
        blockmodels::ResourceLocation model;
        int x = 0;
        int y = 0;
        bool uvlock = false;
    };

    struct BlockstateDefinition {
        /// One or more model refs per variant key (array entries preserve order from JSON).
        std::unordered_map<std::string, std::vector<VariantModelRef>> variants;
    };

    class JsonBlockStateLoader {
    public:
        explicit JsonBlockStateLoader(std::string assetsRootPath);

        /// Absolute-ish path used for `blockstateResourceId` (same rules as \ref GetOrLoadBlockstate). For error messages.
        std::string FormatBlockstatePath(const std::string& blockstateResourceId) const;

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
