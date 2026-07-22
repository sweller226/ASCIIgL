#include <ASCIICraft/world/block/BlockBreakData.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <string>
#include <unordered_map>

namespace blockbreak {
namespace {

struct BreakEntry {
    float hardness = 1.0f;                      // -1 = unbreakable, 0 = instant
    ToolClass preferredTool = ToolClass::None;
    int requiredHarvestLevel = 0;               // 0 = drops even without matching tool
};

/// Hardness values follow vanilla Minecraft. Harvest levels use this game's
/// convention: 0 = hand, 1 = wooden tools, 2 = stone, 3 = iron.
const std::unordered_map<std::string, BreakEntry>& GetBreakTable() {
    static const std::unordered_map<std::string, BreakEntry> table = {
        // name                          hardness  tool                harvest
        {"minecraft:dandelion",         {0.0f,     ToolClass::None,    0}},
        {"minecraft:poppy",             {0.0f,     ToolClass::None,    0}},
        {"minecraft:tall_grass",        {0.0f,     ToolClass::None,    0}},
        {"minecraft:fern",              {0.0f,     ToolClass::None,    0}},

        {"minecraft:dirt",              {0.5f,     ToolClass::Shovel,  0}},
        {"minecraft:grass",             {0.6f,     ToolClass::Shovel,  0}},
        {"minecraft:farmland",          {0.6f,     ToolClass::Shovel,  0}},

        {"minecraft:cobblestone",       {2.0f,     ToolClass::Pickaxe, 1}},
        {"minecraft:stone",             {1.5f,     ToolClass::Pickaxe, 1}},
        {"minecraft:stone_stairs",      {2.0f,     ToolClass::Pickaxe, 1}},
        {"minecraft:cobblestone_slab",  {2.0f,     ToolClass::Pickaxe, 1}},
        {"minecraft:furnace",           {3.5f,     ToolClass::Pickaxe, 1}},

        {"minecraft:oak_log",           {2.0f,     ToolClass::Axe,     0}},
        {"minecraft:oak_planks",        {2.0f,     ToolClass::Axe,     0}},
        {"minecraft:oak_slab",          {2.0f,     ToolClass::Axe,     0}},
        {"minecraft:oak_stairs",        {2.0f,     ToolClass::Axe,     0}},
        {"minecraft:fence",             {2.0f,     ToolClass::Axe,     0}},
        {"minecraft:crafting_table",    {2.5f,     ToolClass::Axe,     0}},
        {"minecraft:bookshelf",         {1.5f,     ToolClass::Axe,     0}},

        {"minecraft:oak_leaves",        {0.2f,     ToolClass::None,    0}},
        {"minecraft:glass",             {0.3f,     ToolClass::None,    0}},
        {"minecraft:blue_wool",         {0.8f,     ToolClass::None,    0}},
        {"minecraft:green_wool",        {0.8f,     ToolClass::None,    0}},

        // Fluids are not mineable.
        {"minecraft:water",             {-1.0f,    ToolClass::None,    0}},
    };
    return table;
}

} // namespace

void ApplyBlockBreakData(blockstate::BlockStateRegistry& bsr) {
    const auto& table = GetBreakTable();

    for (uint16_t typeId = 0; typeId < bsr.GetTotalTypeCount(); ++typeId) {
        blockstate::BlockType& type = bsr.GetTypeMutable(typeId);
        if (type.name == "minecraft:air") {
            continue;
        }

        auto it = table.find(type.name);
        if (it == table.end()) {
            ASCIIgL::Logger::Warning(
                "BlockBreakData: no break entry for '" + type.name + "', using defaults");
            continue;
        }

        type.hardness = it->second.hardness;
        type.preferredTool = it->second.preferredTool;
        type.requiredHarvestLevel = it->second.requiredHarvestLevel;
    }
}

} // namespace blockbreak
