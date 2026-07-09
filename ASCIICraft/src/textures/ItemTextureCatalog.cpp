#include <ASCIICraft/textures/ItemTextureCatalog.hpp>

namespace itemtextures {

const std::vector<CatalogEntry>& GetItemTextureCatalog() {
    static const std::vector<CatalogEntry> kCatalog = {
        // Resources / materials
        {"minecraft:items/coal",       "res/textures/items/coal.png",       nullptr, 0.0f},
        {"minecraft:items/iron_ingot", "res/textures/items/iron_ingot.png", nullptr, 0.0f},
        {"minecraft:items/gold_ingot", "res/textures/items/gold_ingot.png", nullptr, 0.0f},
        {"minecraft:items/stick",      "res/textures/items/stick.png",      nullptr, 0.0f},
        // Swords
        {"minecraft:items/wood_sword",  "res/textures/items/wood_sword.png",  nullptr, 0.0f},
        {"minecraft:items/stone_sword", "res/textures/items/stone_sword.png"},
        {"minecraft:items/iron_sword",  "res/textures/items/iron_sword.png",  nullptr, 0.0f},
        // Shovels
        {"minecraft:items/wood_shovel",  "res/textures/items/wood_shovel.png",  nullptr, 0.0f},
        {"minecraft:items/stone_shovel", "res/textures/items/stone_shovel.png", nullptr, 0.0f},
        {"minecraft:items/iron_shovel",  "res/textures/items/iron_shovel.png",  nullptr, 0.0f},
        // Pickaxes
        {"minecraft:items/wood_pickaxe",  "res/textures/items/wood_pickaxe.png",  nullptr, 0.0f},
        {"minecraft:items/stone_pickaxe", "res/textures/items/stone_pickaxe.png"},
        {"minecraft:items/iron_pickaxe",  "res/textures/items/iron_pickaxe.png",  nullptr, 0.0f},
        // Axes
        {"minecraft:items/wood_axe",  "res/textures/items/wood_axe.png",  nullptr, 0.0f},
        {"minecraft:items/stone_axe", "res/textures/items/stone_axe.png"},
        {"minecraft:items/iron_axe",  "res/textures/items/iron_axe.png",  nullptr, 0.0f},
        {"minecraft:items/bread", "res/textures/items/bread.png", nullptr, 0.0f},
    };
    return kCatalog;
}

} // namespace itemtextures
