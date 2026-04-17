#include <ASCIICraft/world/BlockTextureCatalog.hpp>

namespace blocktextures {

const std::vector<CatalogEntry>& GetBlockTextureCatalog() {
    static const std::vector<CatalogEntry> kCatalog = {
        {"minecraft:blocks/stone",             "res/textures/Splotch/assets/minecraft/textures/blocks/stone.png"},
        {"minecraft:blocks/cobblestone",       "res/textures/Splotch/assets/minecraft/textures/blocks/cobblestone.png"},
        {"minecraft:blocks/bedrock",           "res/textures/Splotch/assets/minecraft/textures/blocks/bedrock.png"},
        {"minecraft:blocks/dirt",              "res/textures/Splotch/assets/minecraft/textures/blocks/dirt.png"},
        {"minecraft:blocks/log_oak",           "res/textures/Splotch/assets/minecraft/textures/blocks/log_oak.png"},
        {"minecraft:blocks/log_oak_top",       "res/textures/Splotch/assets/minecraft/textures/blocks/log_oak_top.png"},
        {"minecraft:blocks/leaves_oak",        "res/textures/Splotch/assets/minecraft/textures/blocks/leaves_oak.png"},
        {"minecraft:blocks/planks_oak",        "res/textures/Splotch/assets/minecraft/textures/blocks/planks_oak.png"},
        {"minecraft:blocks/grass_top",         "res/textures/Splotch/assets/minecraft/textures/blocks/grass_top.png"},
        {"minecraft:blocks/grass_side",        "res/textures/Splotch/assets/minecraft/textures/blocks/grass_side.png"},
        {"minecraft:blocks/water_still",       "res/textures/Splotch/assets/minecraft/textures/blocks/water_still.png"},
        {"minecraft:blocks/crafting_table_front","res/textures/Splotch/assets/minecraft/textures/blocks/crafting_table_front.png"},
        {"minecraft:blocks/crafting_table_side","res/textures/Splotch/assets/minecraft/textures/blocks/crafting_table_side.png"},
        {"minecraft:blocks/crafting_table_top","res/textures/Splotch/assets/minecraft/textures/blocks/crafting_table_top.png"},
        {"minecraft:blocks/furnace_front_off", "res/textures/Splotch/assets/minecraft/textures/blocks/furnace_front_off.png"},
        {"minecraft:blocks/furnace_front_on",  "res/textures/Splotch/assets/minecraft/textures/blocks/furnace_front_on.png"},
        {"minecraft:blocks/furnace_side",      "res/textures/Splotch/assets/minecraft/textures/blocks/furnace_side.png"},
        {"minecraft:blocks/furnace_top",       "res/textures/Splotch/assets/minecraft/textures/blocks/furnace_top.png"},
        {"minecraft:blocks/glass",             "res/textures/Splotch/assets/minecraft/textures/blocks/glass.png"},
        {"minecraft:blocks/bookshelf",         "res/textures/Splotch/assets/minecraft/textures/blocks/bookshelf.png"},
        {"minecraft:blocks/sand",              "res/textures/Splotch/assets/minecraft/textures/blocks/sand.png"},
        {"minecraft:blocks/gravel",            "res/textures/Splotch/assets/minecraft/textures/blocks/gravel.png"},
        {"minecraft:blocks/brick",             "res/textures/Splotch/assets/minecraft/textures/blocks/brick.png"},
        {"minecraft:blocks/torch_on",          "res/textures/Splotch/assets/minecraft/textures/blocks/torch_on.png"},
        {"minecraft:blocks/flower_dandelion",  "res/textures/Splotch/assets/minecraft/textures/blocks/flower_dandelion.png"},
    };
    return kCatalog;
}

std::vector<std::string> BuildBlockTexturePaths() {
    std::vector<std::string> out;
    const auto& catalog = GetBlockTextureCatalog();
    out.reserve(catalog.size());
    for (const auto& e : catalog) {
        out.emplace_back(e.filePath);
    }
    return out;
}

std::unordered_map<std::string, int> BuildTextureIdToLayerMap() {
    std::unordered_map<std::string, int> out;
    const auto& catalog = GetBlockTextureCatalog();
    out.reserve(catalog.size());
    for (size_t i = 0; i < catalog.size(); ++i) {
        out[std::string(catalog[i].textureId)] = static_cast<int>(i);
    }
    return out;
}

std::string CanonicalizeTextureId(const std::string& textureId) {
    if (textureId.find(':') != std::string::npos) {
        return textureId;
    }
    return "minecraft:" + textureId;
}

int GetLayerForTextureId(const std::string& textureId) {
    static const std::unordered_map<std::string, int> kMap = BuildTextureIdToLayerMap();
    auto it = kMap.find(CanonicalizeTextureId(textureId));
    if (it == kMap.end()) return -1;
    return it->second;
}

} // namespace blocktextures

