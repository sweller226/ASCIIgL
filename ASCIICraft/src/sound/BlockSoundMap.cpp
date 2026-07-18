#include <ASCIICraft/sound/BlockSoundMap.hpp>

#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace sound {

namespace {
const std::string kDefaultFamily = "stone";
}

std::string BlockSoundMap::ResolveStepSoundId(
    uint16_t typeId,
    const blockstate::BlockStateRegistry& bsr
) {
    return "step." + ResolveFamily(typeId, bsr);
}

std::string BlockSoundMap::ResolveDigSoundId(
    uint16_t typeId,
    const blockstate::BlockStateRegistry& bsr
) {
    return "dig." + ResolveFamily(typeId, bsr);
}

const std::string& BlockSoundMap::ResolveFamily(
    uint16_t typeId,
    const blockstate::BlockStateRegistry& bsr
) {
    if (!m_built) {
        Build(bsr);
    }

    const auto it = m_familyByTypeId.find(typeId);
    if (it != m_familyByTypeId.end()) {
        return it->second;
    }

    // Name-based fallback, cached so the string search runs once per type.
    std::string resolved = kDefaultFamily;
    if (typeId < bsr.GetTotalTypeCount()) {
        const std::string& name = bsr.GetType(typeId).name;
        if (name.find("grass") != std::string::npos || name.find("leaves") != std::string::npos) {
            resolved = "grass";
        } else if (name.find("dirt") != std::string::npos || name.find("gravel") != std::string::npos) {
            resolved = "gravel";
        } else if (name.find("log") != std::string::npos || name.find("planks") != std::string::npos ||
                   name.find("wood") != std::string::npos || name.find("fence") != std::string::npos) {
            resolved = "wood";
        } else if (name.find("sand") != std::string::npos) {
            resolved = "sand";
        } else if (name.find("snow") != std::string::npos) {
            resolved = "snow";
        } else if (name.find("wool") != std::string::npos || name.find("carpet") != std::string::npos) {
            resolved = "cloth";
        }
    }

    return m_familyByTypeId.emplace(typeId, std::move(resolved)).first->second;
}

void BlockSoundMap::MapType(
    const blockstate::BlockStateRegistry& bsr,
    const char* typeName,
    const char* family
) {
    const uint16_t typeId = bsr.GetTypeId(typeName);
    m_familyByTypeId[typeId] = family;
}

void BlockSoundMap::Build(const blockstate::BlockStateRegistry& bsr) {
    m_familyByTypeId.clear();

    MapType(bsr, "minecraft:grass", "grass");
    MapType(bsr, "minecraft:dirt", "gravel");
    MapType(bsr, "minecraft:tall_grass", "grass");
    MapType(bsr, "minecraft:fern", "grass");
    MapType(bsr, "minecraft:dandelion", "grass");
    MapType(bsr, "minecraft:poppy", "grass");

    MapType(bsr, "minecraft:oak_stairs", "wood");
    MapType(bsr, "minecraft:stone_stairs", "stone");
    MapType(bsr, "minecraft:cobblestone", "stone");
    MapType(bsr, "minecraft:cobblestone_slab", "stone");
    MapType(bsr, "minecraft:glass", "stone");

    MapType(bsr, "minecraft:oak_log", "wood");
    MapType(bsr, "minecraft:oak_planks", "wood");
    MapType(bsr, "minecraft:oak_slab", "wood");
    MapType(bsr, "minecraft:fence", "wood");
    MapType(bsr, "minecraft:crafting_table", "wood");
    MapType(bsr, "minecraft:bookshelf", "wood");
    MapType(bsr, "minecraft:furnace", "wood");

    MapType(bsr, "minecraft:oak_leaves", "grass");

    MapType(bsr, "minecraft:blue_wool", "cloth");
    MapType(bsr, "minecraft:green_wool", "cloth");

    m_built = true;
}

} // namespace sound
