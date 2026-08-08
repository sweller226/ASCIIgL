#include <ASCIICraft/world/chunk/V1StateIdRemap.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

#include <ASCIIgL/util/Logger.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace v1_state_id {
namespace {

// Block registration order as it stood when v1 blobs were written (minecraft:stone
// omitted - it was inserted later). Entries are resolved by NAME against the live
// registry, so current registration order may change freely; only this list and the
// names in it are fixed. Property cardinalities must match the live registry.
constexpr const char* kV1TypeOrder[] = {
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

std::vector<uint32_t> g_v1ToCurrent;

} // namespace

void BuildRemapTable(const blockstate::BlockStateRegistry& bsr) {
    g_v1ToCurrent.clear();
    g_v1ToCurrent.reserve(bsr.GetTotalStateCount());

    uint32_t v1CobbleId = UINT32_MAX;
    uint32_t v1StoneStairsBase = UINT32_MAX;

    for (const char* typeName : kV1TypeOrder) {
        const uint16_t typeId = bsr.GetTypeId(typeName);
        if (typeId == 0 && std::string(typeName) != "minecraft:air") {
            ASCIIgL::Logger::Error(
                std::string("V1StateIdRemap: missing v1 type '") + typeName + "'"
            );
            continue;
        }
        const blockstate::BlockType& type = bsr.GetType(typeId);
        if (std::strcmp(typeName, "minecraft:cobblestone") == 0) {
            v1CobbleId = static_cast<uint32_t>(g_v1ToCurrent.size());
        } else if (std::strcmp(typeName, "minecraft:stone_stairs") == 0) {
            v1StoneStairsBase = static_cast<uint32_t>(g_v1ToCurrent.size());
        }
        for (uint32_t i = 0; i < type.stateCount; ++i) {
            g_v1ToCurrent.push_back(type.baseStateId + i);
        }
    }

    // Stone was inserted after cobblestone; v1 worlds must still map these correctly.
    // These run on every launch and are the cheap guard against a block rename
    // silently breaking v1 loads.
    if (v1CobbleId != UINT32_MAX) {
        const uint32_t expected = bsr.GetDefaultState("minecraft:cobblestone");
        if (Remap(v1CobbleId) != expected) {
            ASCIIgL::Logger::Error("V1StateIdRemap: cobblestone remap mismatch");
        }
    }
    if (v1StoneStairsBase != UINT32_MAX) {
        const uint32_t expected = bsr.GetDefaultState("minecraft:stone_stairs");
        if (Remap(v1StoneStairsBase) != expected) {
            ASCIIgL::Logger::Error("V1StateIdRemap: stone_stairs remap mismatch");
        }
    }

    ASCIIgL::Logger::Info(
        "V1StateIdRemap: remap table built (" +
        std::to_string(g_v1ToCurrent.size()) + " v1 stateIds)"
    );
}

uint32_t Remap(uint32_t v1StateId) {
    if (v1StateId >= g_v1ToCurrent.size()) {
        return blockstate::BlockStateRegistry::AIR_STATE_ID;
    }
    return g_v1ToCurrent[v1StateId];
}

} // namespace v1_state_id
