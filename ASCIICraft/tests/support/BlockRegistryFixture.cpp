#include "support/BlockRegistryFixture.hpp"

#include <ASCIICraft/world/block/VanillaBlockRegistration.hpp>
#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>

namespace testsupport {

entt::registry& SharedRegistry() {
    static entt::registry registry = [] {
        entt::registry r;
        // Asset root stays "res", which the test executable stages next to itself and
        // runs from (CTest sets WORKING_DIRECTORY to the binary directory).
        blockstate::RegisterVanillaBlocksInContext(r);
        return r;
    }();
    return registry;
}

const blockstate::BlockStateRegistry& SharedBlocks() {
    return SharedRegistry().ctx().get<blockstate::BlockStateRegistry>();
}

const blockmodels::BlockModelLibrary& SharedModels() {
    return SharedRegistry().ctx().get<blockmodels::BlockModelLibrary>();
}

const VanillaIds& Ids() {
    static const VanillaIds ids = [] {
        const auto& bsr = SharedBlocks();
        VanillaIds v;
        v.air       = bsr.GetDefaultState("minecraft:air");
        v.dirt      = bsr.GetDefaultState("minecraft:dirt");
        v.grass     = bsr.GetDefaultState("minecraft:grass");
        v.stone     = bsr.GetDefaultState("minecraft:stone");
        v.water     = bsr.GetDefaultState("minecraft:water");
        v.oakLog    = bsr.GetDefaultState("minecraft:oak_log");
        v.oakLeaves = bsr.GetDefaultState("minecraft:oak_leaves");
        v.poppy     = bsr.GetDefaultState("minecraft:poppy");
        v.dandelion = bsr.GetDefaultState("minecraft:dandelion");
        v.fern      = bsr.GetDefaultState("minecraft:fern");
        v.tallGrass = bsr.GetDefaultState("minecraft:tall_grass");
        v.glass     = bsr.GetDefaultState("minecraft:glass");
        return v;
    }();
    return ids;
}

} // namespace testsupport
