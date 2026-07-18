#include <ASCIICraft/textures/BlockTextureCatalog.hpp>

#include <glm/vec3.hpp>

namespace blocktextures {

namespace {

// Palette-index hue directions for MC tint-mask textures (tintindex 0).
const glm::ivec3 kGrassTintHue(131, 178, 83);

} // namespace

const std::vector<CatalogEntry>& GetBlockTextureCatalog() {
    static const std::vector<CatalogEntry> kCatalog = {
        {"minecraft:blocks/dirt",              "res/textures/blocks/dirt.png"},
        {"minecraft:blocks/cobblestone",       "res/textures/blocks/cobblestone.png", nullptr, 0.0f},
        {"minecraft:blocks/log_oak",           "res/textures/blocks/log_oak.png"},
        {"minecraft:blocks/log_oak_top",       "res/textures/blocks/log_oak_top.png"},
        {"minecraft:blocks/leaves_oak",        "res/textures/blocks/leaves_oak.png", &kGrassTintHue},
        {"minecraft:blocks/planks_oak",        "res/textures/blocks/planks_oak.png"},
        {"minecraft:blocks/grass_top",         "res/textures/blocks/grass_top.png", &kGrassTintHue},
        {"minecraft:blocks/grass_side",        "res/textures/blocks/grass_side.png"},
        {"minecraft:blocks/grass_side_overlay", "res/textures/blocks/grass_side_overlay.png", &kGrassTintHue},
        {"minecraft:blocks/water_still",       "res/textures/blocks/water_still.png", nullptr, 1.0f},
        {"minecraft:blocks/water_still_color",       "res/textures/blocks/water_still_color.png"},
        {"minecraft:blocks/crafting_table_front","res/textures/blocks/crafting_table_front.png", nullptr, 1.0f},
        {"minecraft:blocks/crafting_table_side","res/textures/blocks/crafting_table_side.png", nullptr, 1.0f},
        {"minecraft:blocks/crafting_table_top","res/textures/blocks/crafting_table_top.png", nullptr, 0.0f},
        {"minecraft:blocks/furnace_front_off", "res/textures/blocks/furnace_front_off.png"},
        {"minecraft:blocks/furnace_side",      "res/textures/blocks/furnace_side.png",      nullptr, 1.0f},
        {"minecraft:blocks/furnace_top",       "res/textures/blocks/furnace_top.png",       nullptr, 0.0f},
        {"minecraft:blocks/glass",             "res/textures/blocks/glass.png"},
        {"minecraft:blocks/bookshelf",         "res/textures/blocks/bookshelf.png"},
        {"minecraft:blocks/flower_dandelion",  "res/textures/blocks/flower_dandelion.png"},
        {"minecraft:blocks/flower_rose",       "res/textures/blocks/flower_rose.png"},
        {"minecraft:blocks/fern",              "res/textures/blocks/fern.png", &kGrassTintHue},
        {"minecraft:blocks/tallgrass",         "res/textures/blocks/tallgrass.png", &kGrassTintHue},
        {"minecraft:blocks/wool_colored_blue", "res/textures/blocks/blue_wool.png", nullptr, 0.0f},
        {"minecraft:blocks/wool_colored_green", "res/textures/blocks/green_wool.png", nullptr, 0.0f},
        // Mining crack overlay stages (paletteWeight 0: don't skew palette clustering).
        {"minecraft:blocks/destroy_stage_0",   "res/textures/blocks/destroy_stage_0.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_1",   "res/textures/blocks/destroy_stage_1.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_2",   "res/textures/blocks/destroy_stage_2.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_3",   "res/textures/blocks/destroy_stage_3.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_4",   "res/textures/blocks/destroy_stage_4.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_5",   "res/textures/blocks/destroy_stage_5.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_6",   "res/textures/blocks/destroy_stage_6.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_7",   "res/textures/blocks/destroy_stage_7.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_8",   "res/textures/blocks/destroy_stage_8.png", nullptr, 0.0f},
        {"minecraft:blocks/destroy_stage_9",   "res/textures/blocks/destroy_stage_9.png", nullptr, 0.0f},
    };
    return kCatalog;
}

} // namespace blocktextures
