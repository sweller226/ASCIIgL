#include <ASCIICraft/textures/ItemTextureCatalog.hpp>

namespace itemtextures {

const std::vector<CatalogEntry>& GetItemTextureCatalog() {
    static const std::vector<CatalogEntry> kCatalog = {
        // Resources / materials
        {"minecraft:items/coal",       "res/textures/items/coal.png"},
        {"minecraft:items/iron_ingot", "res/textures/items/iron_ingot.png"},
        {"minecraft:items/gold_ingot", "res/textures/items/gold_ingot.png"},
        {"minecraft:items/stick",      "res/textures/items/stick.png"},
        // Swords
        {"minecraft:items/wood_sword",  "res/textures/items/wood_sword.png"},
        {"minecraft:items/stone_sword", "res/textures/items/stone_sword.png"},
        {"minecraft:items/iron_sword",  "res/textures/items/iron_sword.png"},
        // Shovels
        {"minecraft:items/wood_shovel",  "res/textures/items/wood_shovel.png"},
        {"minecraft:items/stone_shovel", "res/textures/items/stone_shovel.png"},
        {"minecraft:items/iron_shovel",  "res/textures/items/iron_shovel.png"},
        // Pickaxes
        {"minecraft:items/wood_pickaxe",  "res/textures/items/wood_pickaxe.png"},
        {"minecraft:items/stone_pickaxe", "res/textures/items/stone_pickaxe.png"},
        {"minecraft:items/iron_pickaxe",  "res/textures/items/iron_pickaxe.png"},
        // Axes
        {"minecraft:items/wood_axe",  "res/textures/items/wood_axe.png"},
        {"minecraft:items/stone_axe", "res/textures/items/stone_axe.png"},
        {"minecraft:items/iron_axe",  "res/textures/items/iron_axe.png"},
        {"minecraft:items/bread", "res/textures/items/bread.png"},
    };
    return kCatalog;
}

} // namespace itemtextures
