#include <ASCIICraft/textures/BlockTextureCatalog.hpp>

#include <glm/vec3.hpp>

namespace blocktextures {

namespace {

// Palette-index hue directions for MC tint-mask textures (tintindex 0).
const glm::ivec3 kGrassTintHue(131, 178, 83);

} // namespace

const std::vector<CatalogEntry>& GetBlockTextureCatalog() {
    static const std::vector<CatalogEntry> kCatalog = {
        {"minecraft:blocks/stone",             "res/textures/blocks/stone.png"},
        {"minecraft:blocks/cobblestone",       "res/textures/blocks/cobblestone.png"},
        {"minecraft:blocks/bedrock",           "res/textures/blocks/bedrock.png"},
        {"minecraft:blocks/dirt",              "res/textures/blocks/dirt.png"},
        {"minecraft:blocks/log_oak",           "res/textures/blocks/log_oak.png"},
        {"minecraft:blocks/log_oak_top",       "res/textures/blocks/log_oak_top.png"},
        {"minecraft:blocks/leaves_oak",        "res/textures/blocks/leaves_oak.png", &kGrassTintHue},
        {"minecraft:blocks/planks_oak",        "res/textures/blocks/planks_oak.png"},
        {"minecraft:blocks/grass_top",         "res/textures/blocks/grass_top.png", &kGrassTintHue},
        {"minecraft:blocks/grass_side",        "res/textures/blocks/grass_side.png"},
        {"minecraft:blocks/grass_side_overlay", "res/textures/blocks/grass_side_overlay.png", &kGrassTintHue},
        // {"minecraft:blocks/grass_side_snowed",   "res/textures/blocks/grass_side_snowed.png"},  // modifed json for snow
        {"minecraft:blocks/water_still",       "res/textures/blocks/water_still.png"},
        {"minecraft:blocks/crafting_table_front","res/textures/blocks/crafting_table_front.png"},
        {"minecraft:blocks/crafting_table_side","res/textures/blocks/crafting_table_side.png"},
        {"minecraft:blocks/crafting_table_top","res/textures/blocks/crafting_table_top.png"},
        {"minecraft:blocks/furnace_front_off", "res/textures/blocks/furnace_front_off.png"},
        // {"minecraft:blocks/furnace_front_on",  "res/textures/blocks/furnace_front_on.png"}, // modified json for lit furnace
        {"minecraft:blocks/furnace_side",      "res/textures/blocks/furnace_side.png"},
        {"minecraft:blocks/furnace_top",       "res/textures/blocks/furnace_top.png"},
        {"minecraft:blocks/glass",             "res/textures/blocks/glass.png"},
        {"minecraft:blocks/bookshelf",         "res/textures/blocks/bookshelf.png"},
        {"minecraft:blocks/torch_on",          "res/textures/blocks/torch_on.png"},
        {"minecraft:blocks/flower_dandelion",  "res/textures/blocks/flower_dandelion.png"},
        {"minecraft:blocks/flower_rose",       "res/textures/blocks/flower_rose.png"},
        {"minecraft:blocks/fern",              "res/textures/blocks/fern.png", &kGrassTintHue},
        {"minecraft:blocks/tallgrass",         "res/textures/blocks/tallgrass.png", &kGrassTintHue},
    };
    return kCatalog;
}

} // namespace blocktextures
