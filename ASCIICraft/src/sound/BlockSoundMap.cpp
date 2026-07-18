#include <ASCIICraft/sound/BlockSoundMap.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace sound {

namespace {
const std::string kDefaultStepSoundId = "step.stone";
}

const std::string& BlockSoundMap::ResolveStepSoundId(
    uint16_t typeId,
    const blockstate::BlockStateRegistry& bsr
) {
    if (!m_built) {
        Build(bsr);
    }

    const auto it = m_soundByTypeId.find(typeId);
    if (it != m_soundByTypeId.end()) {
        return it->second;
    }

    // Name-based fallback, cached so the string search runs once per type.
    std::string resolved = kDefaultStepSoundId;
    if (typeId < bsr.GetTotalTypeCount()) {
        const std::string& name = bsr.GetType(typeId).name;
        if (name.find("grass") != std::string::npos || name.find("leaves") != std::string::npos) {
            resolved = "step.grass";
        } else if (name.find("dirt") != std::string::npos || name.find("gravel") != std::string::npos) {
            resolved = "step.gravel";
        } else if (name.find("log") != std::string::npos || name.find("planks") != std::string::npos ||
                   name.find("wood") != std::string::npos || name.find("fence") != std::string::npos) {
            resolved = "step.wood";
        } else if (name.find("sand") != std::string::npos) {
            resolved = "step.sand";
        } else if (name.find("snow") != std::string::npos) {
            resolved = "step.snow";
        } else if (name.find("wool") != std::string::npos || name.find("carpet") != std::string::npos) {
            resolved = "step.cloth";
        }
    }

    return m_soundByTypeId.emplace(typeId, std::move(resolved)).first->second;
}

void BlockSoundMap::MapType(
    const blockstate::BlockStateRegistry& bsr,
    const char* typeName,
    const char* soundId
) {
    const uint16_t typeId = bsr.GetTypeId(typeName);
    m_soundByTypeId[typeId] = soundId;
}

void BlockSoundMap::Build(const blockstate::BlockStateRegistry& bsr) {
    m_soundByTypeId.clear();

    MapType(bsr, "minecraft:grass", "step.grass");
    MapType(bsr, "minecraft:dirt", "step.gravel");
    MapType(bsr, "minecraft:tall_grass", "step.grass");
    MapType(bsr, "minecraft:fern", "step.grass");
    MapType(bsr, "minecraft:dandelion", "step.grass");
    MapType(bsr, "minecraft:poppy", "step.grass");

    MapType(bsr, "minecraft:oak_stairs", "step.wood");
    MapType(bsr, "minecraft:stone_stairs", "step.stone");
    MapType(bsr, "minecraft:cobblestone", "step.stone");
    MapType(bsr, "minecraft:cobblestone_slab", "step.stone");
    MapType(bsr, "minecraft:glass", "step.stone");

    MapType(bsr, "minecraft:oak_log", "step.wood");
    MapType(bsr, "minecraft:oak_planks", "step.wood");
    MapType(bsr, "minecraft:oak_slab", "step.wood");
    MapType(bsr, "minecraft:fence", "step.wood");
    MapType(bsr, "minecraft:crafting_table", "step.wood");
    MapType(bsr, "minecraft:bookshelf", "step.wood");
    MapType(bsr, "minecraft:furnace", "step.wood");

    MapType(bsr, "minecraft:oak_leaves", "step.grass");

    m_built = true;
}

} // namespace sound
