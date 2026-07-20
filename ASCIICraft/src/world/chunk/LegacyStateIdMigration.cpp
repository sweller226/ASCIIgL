#include <ASCIICraft/world/chunk/LegacyStateIdMigration.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace legacy_state_id {
namespace {

// Pre-stone registration order from Game::InitializeBlockStates (minecraft:stone omitted).
// Property cardinalities must match the live registry for each type.
constexpr const char* kLegacyTypeOrder[] = {
    "minecraft:air",
    "minecraft:dandelion",
    "minecraft:poppy",
    "minecraft:tall_grass",
    "minecraft:fern",
    "minecraft:fence",
    "minecraft:oak_stairs",
    "minecraft:cobblestone",
    "minecraft:stone_stairs",
    "minecraft:dirt",
    "minecraft:grass",
    "minecraft:oak_log",
    "minecraft:oak_planks",
    "minecraft:oak_slab",
    "minecraft:cobblestone_slab",
    "minecraft:oak_leaves",
    "minecraft:crafting_table",
    "minecraft:bookshelf",
    "minecraft:furnace",
    "minecraft:glass",
    "minecraft:blue_wool",
    "minecraft:green_wool",
    "minecraft:water",
};

std::vector<uint32_t> g_legacyToCurrent;

} // namespace

void BuildRemapTable(const blockstate::BlockStateRegistry& bsr) {
    g_legacyToCurrent.clear();
    g_legacyToCurrent.reserve(bsr.GetTotalStateCount());

    uint32_t legacyCobbleId = UINT32_MAX;
    uint32_t legacyStoneStairsBase = UINT32_MAX;

    for (const char* typeName : kLegacyTypeOrder) {
        const uint16_t typeId = bsr.GetTypeId(typeName);
        if (typeId == 0 && std::string(typeName) != "minecraft:air") {
            ASCIIgL::Logger::Error(
                std::string("LegacyStateIdMigration: missing legacy type '") + typeName + "'"
            );
            continue;
        }
        const blockstate::BlockType& type = bsr.GetType(typeId);
        if (std::strcmp(typeName, "minecraft:cobblestone") == 0) {
            legacyCobbleId = static_cast<uint32_t>(g_legacyToCurrent.size());
        } else if (std::strcmp(typeName, "minecraft:stone_stairs") == 0) {
            legacyStoneStairsBase = static_cast<uint32_t>(g_legacyToCurrent.size());
        }
        for (uint32_t i = 0; i < type.stateCount; ++i) {
            g_legacyToCurrent.push_back(type.baseStateId + i);
        }
    }

    // Stone was inserted after cobblestone; v1 worlds must still map these correctly.
    if (legacyCobbleId != UINT32_MAX) {
        const uint32_t expected = bsr.GetDefaultState("minecraft:cobblestone");
        if (RemapLegacyStateId(legacyCobbleId) != expected) {
            ASCIIgL::Logger::Error("LegacyStateIdMigration: cobblestone remap mismatch");
        }
    }
    if (legacyStoneStairsBase != UINT32_MAX) {
        const uint32_t expected = bsr.GetDefaultState("minecraft:stone_stairs");
        if (RemapLegacyStateId(legacyStoneStairsBase) != expected) {
            ASCIIgL::Logger::Error("LegacyStateIdMigration: stone_stairs remap mismatch");
        }
    }

    ASCIIgL::Logger::Info(
        "LegacyStateIdMigration: remap table built (" +
        std::to_string(g_legacyToCurrent.size()) + " legacy stateIds)"
    );
}

uint32_t RemapLegacyStateId(uint32_t legacyStateId) {
    if (legacyStateId >= g_legacyToCurrent.size()) {
        return blockstate::BlockStateRegistry::AIR_STATE_ID;
    }
    return g_legacyToCurrent[legacyStateId];
}

} // namespace legacy_state_id
