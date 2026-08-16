#include <ASCIICraft/world/block/VanillaBlockRegistration.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include <ASCIIgL/util/Logger.hpp>

#include <ASCIICraft/world/block/BlockBreakData.hpp>
#include <ASCIICraft/world/block/models/BlockModelLibrary.hpp>
#include <ASCIICraft/world/block/models/JsonModelLoader.hpp>
#include <ASCIICraft/world/block/models/WaterModelBuilder.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/JsonBlockModelRegistration.hpp>
#include <ASCIICraft/world/block/state/JsonBlockStateLoader.hpp>
#include <ASCIICraft/world/block/state/VariantKey.hpp>
#include <ASCIICraft/world/chunk/V1StateIdRemap.hpp>

namespace blockstate {

void RegisterVanillaBlocks(BlockStateRegistry& bsr,
                           blockmodels::BlockModelLibrary& modelLibrary,
                           const VanillaRegistrationOptions& opts) {
    // ======================== Block model registration ========================
    // 1) Air first (typeId=0, stateId=0). 2) Shared JSON loaders for one assets root.
    // 3) Types + derived render data. 4) Models via JsonBlockModelRegistration (1.8.9 assets),
    //    except air (no mesh) and water (no blockstate in pack — procedural mesh).
    //
    // Order determines state id assignment. Air must stay first (typeId 0 / stateId 0);
    // see the header for why the rest is less fragile than it looks.
    bsr.RegisterType("minecraft:air", {});
    const uint16_t airType = bsr.GetTypeId("minecraft:air");
    bsr.SetDerivedData(airType, [](blockstate::BlockState& s) {
        s.isRenderable = false;
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Translucent;
    });
    modelLibrary.RegisterModel(airType, nullptr, bsr);

    const std::string& kVanillaBlockAssetRoot = opts.assetRoot;
    blockstate::JsonBlockStateLoader blockstateLoader(kVanillaBlockAssetRoot);
    blockmodels::JsonModelLoader jsonModelLoader(kVanillaBlockAssetRoot);

    const auto registerJsonBackedOrLog = [&](const char* typeName) {
        if (!blockstate::RegisterJsonBackedBlockType(
                typeName,
                bsr,
                modelLibrary,
                blockstateLoader,
                jsonModelLoader
            )) {
            ASCIIgL::Logger::Error(
                std::string("RegisterVanillaBlocks: JSON model registration failed for ") + typeName
            );
        }
    };

    const auto registerOpaqueJsonBacked = [&](const char* typeName) {
        bsr.RegisterType(typeName, {});
        const uint16_t tid = bsr.GetTypeId(typeName);
        bsr.SetDerivedData(tid, [](blockstate::BlockState& s) { s.renderMode = blockstate::RenderMode::Opaque; });
        registerJsonBackedOrLog(typeName);
    };

    // === Terrain & plants (vanilla blockstate + block models under res/blockstates and res/models) ===
    bsr.RegisterType("minecraft:dandelion", {});
    const uint16_t dandelionType = bsr.GetTypeId("minecraft:dandelion");
    bsr.SetDerivedData(dandelionType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:dandelion");

    bsr.RegisterType("minecraft:poppy", {});
    const uint16_t poppyType = bsr.GetTypeId("minecraft:poppy");
    bsr.SetDerivedData(poppyType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:poppy");

    bsr.RegisterType("minecraft:tall_grass", {});
    const uint16_t tallGrassType = bsr.GetTypeId("minecraft:tall_grass");
    bsr.SetDerivedData(tallGrassType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:tall_grass");

    bsr.RegisterType("minecraft:fern", {});
    const uint16_t fernType = bsr.GetTypeId("minecraft:fern");
    bsr.SetDerivedData(fernType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:fern");

    // Oak fence geometry in 1.8.9 uses blockstate id `minecraft:fence` (see assets/.../blockstates/fence.json).
    bsr.RegisterType("minecraft:fence", {
        blockstate::BlockProperty{ "east", { "false", "true" }},
        blockstate::BlockProperty{ "north", { "false", "true" }},
        blockstate::BlockProperty{ "south", { "false", "true" }},
        blockstate::BlockProperty{ "west", { "false", "true" }},
    });
    const uint16_t fenceType = bsr.GetTypeId("minecraft:fence");
    bsr.SetDerivedData(fenceType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:fence");

    bsr.RegisterType("minecraft:oak_stairs", {
        blockstate::BlockProperty{ "facing", { "east", "west", "south", "north" } },
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
        blockstate::BlockProperty{ "shape", { "straight", "outer_right", "outer_left", "inner_right", "inner_left" } },
    });
    const uint16_t oakStairsType = bsr.GetTypeId("minecraft:oak_stairs");
    bsr.SetDerivedData(oakStairsType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:oak_stairs");

    registerOpaqueJsonBacked("minecraft:cobblestone");
    registerOpaqueJsonBacked("minecraft:mossy_cobblestone");
    registerOpaqueJsonBacked("minecraft:stone");
    registerOpaqueJsonBacked("minecraft:stonebrick");
    registerOpaqueJsonBacked("minecraft:mossy_stonebrick");

    bsr.RegisterType("minecraft:stone_stairs", {
        blockstate::BlockProperty{ "facing", { "east", "west", "south", "north" } },
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
        blockstate::BlockProperty{ "shape", { "straight", "outer_right", "outer_left", "inner_right", "inner_left" } },
    });
    const uint16_t stoneStairsType = bsr.GetTypeId("minecraft:stone_stairs");
    bsr.SetDerivedData(stoneStairsType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:stone_stairs");

    bsr.RegisterType("minecraft:mossy_cobblestone_stairs", {
        blockstate::BlockProperty{ "facing", { "east", "west", "south", "north" } },
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
        blockstate::BlockProperty{ "shape", { "straight", "outer_right", "outer_left", "inner_right", "inner_left" } },
    });
    const uint16_t mossyCobblestoneStairsType = bsr.GetTypeId("minecraft:mossy_cobblestone_stairs");
    bsr.SetDerivedData(mossyCobblestoneStairsType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:mossy_cobblestone_stairs");

    bsr.RegisterType("minecraft:stone_brick_stairs", {
        blockstate::BlockProperty{ "facing", { "east", "west", "south", "north" } },
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
        blockstate::BlockProperty{ "shape", { "straight", "outer_right", "outer_left", "inner_right", "inner_left" } },
    });
    const uint16_t stoneBrickStairsType = bsr.GetTypeId("minecraft:stone_brick_stairs");
    bsr.SetDerivedData(stoneBrickStairsType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:stone_brick_stairs");

    registerOpaqueJsonBacked("minecraft:dirt");

    // Matches assets/minecraft/blockstates/grass.json (snowy + rotated grass_normal variants).
    bsr.RegisterType("minecraft:grass", {
        blockstate::BlockProperty{ "snowy", {"false", "true"}, 0 }
    });
    const uint16_t grassType = bsr.GetTypeId("minecraft:grass");
    bsr.SetDerivedData(grassType, [&](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:grass");

    // Matches assets/minecraft/blockstates/oak_log.json
    bsr.RegisterType("minecraft:oak_log", {
        blockstate::BlockProperty{ "axis", {"y", "z", "x", "none"}, 0 }
    });
    const uint16_t oakLogType = bsr.GetTypeId("minecraft:oak_log");
    bsr.SetDerivedData(oakLogType, [&](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:oak_log");

    // Matches assets/minecraft/blockstates/oak_planks.json
    registerOpaqueJsonBacked("minecraft:oak_planks");

    bsr.RegisterType("minecraft:oak_slab", {
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
    });
    const uint16_t oakSlabType = bsr.GetTypeId("minecraft:oak_slab");
    bsr.SetDerivedData(oakSlabType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:oak_slab");

    bsr.RegisterType("minecraft:cobblestone_slab", {
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
    });
    const uint16_t cobblestoneSlabType = bsr.GetTypeId("minecraft:cobblestone_slab");
    bsr.SetDerivedData(cobblestoneSlabType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:cobblestone_slab");

    bsr.RegisterType("minecraft:mossy_cobblestone_slab", {
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
    });
    const uint16_t mossyCobblestoneSlabType = bsr.GetTypeId("minecraft:mossy_cobblestone_slab");
    bsr.SetDerivedData(mossyCobblestoneSlabType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:mossy_cobblestone_slab");

    bsr.RegisterType("minecraft:stone_brick_slab", {
        blockstate::BlockProperty{ "half", { "bottom", "top" } },
    });
    const uint16_t stoneBrickSlabType = bsr.GetTypeId("minecraft:stone_brick_slab");
    bsr.SetDerivedData(stoneBrickSlabType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:stone_brick_slab");

    bsr.RegisterType("minecraft:oak_leaves", {});
    const uint16_t leavesType = bsr.GetTypeId("minecraft:oak_leaves");
    bsr.SetDerivedData(leavesType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;                  // treat as cutout, not blended
        s.renderMode = blockstate::RenderMode::Cutout;
        s.cullSameType = false;                   // draw faces between leaves (fuller look)
    });
    registerJsonBackedOrLog("minecraft:oak_leaves");

    // === Utility / decor blocks present in current texture array ===
    registerOpaqueJsonBacked("minecraft:crafting_table");
    registerOpaqueJsonBacked("minecraft:bookshelf");

    bsr.RegisterType("minecraft:furnace", {
        blockstate::BlockProperty{ "facing", { "north", "south", "west", "east" } },
        blockstate::BlockProperty{ "lit", { "false", "true" }, 0 },
    });
    const uint16_t furnaceType = bsr.GetTypeId("minecraft:furnace");
    bsr.SetDerivedData(furnaceType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:furnace");

    bsr.RegisterType("minecraft:barrel", {
        blockstate::BlockProperty{ "facing", { "north", "south", "west", "east", "up", "down" } },
        blockstate::BlockProperty{ "open", { "false", "true" }, 0 },
    });
    const uint16_t barrelType = bsr.GetTypeId("minecraft:barrel");
    bsr.SetDerivedData(barrelType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
    });
    registerJsonBackedOrLog("minecraft:barrel");

    bsr.RegisterType("minecraft:glass", {});
    const uint16_t glassType = bsr.GetTypeId("minecraft:glass");
    bsr.SetDerivedData(glassType, [&](blockstate::BlockState& s) {
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Cutout;
    });
    registerJsonBackedOrLog("minecraft:glass");

    registerOpaqueJsonBacked("minecraft:blue_wool");
    registerOpaqueJsonBacked("minecraft:green_wool");

    // Matches assets/minecraft/blockstates/farmland.json (moisture 0–7; 15/16 tall).
    bsr.RegisterType("minecraft:farmland", {
        blockstate::BlockProperty{ "moisture", {"0", "1", "2", "3", "4", "5", "6", "7"}, 0 }
    });
    const uint16_t farmlandType = bsr.GetTypeId("minecraft:farmland");
    bsr.SetDerivedData(farmlandType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:farmland");

    // === Water ===
    bsr.RegisterType("minecraft:water", {
        blockstate::BlockProperty{ "top", {"false", "true"}, 1 }
    });
    const uint16_t waterType = bsr.GetTypeId("minecraft:water");
    bsr.SetDerivedData(waterType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = true;
        s.renderMode = blockstate::RenderMode::Translucent;
        s.isFullBlock = !(bsr.GetPropertyValue(s.stateId, "top") == "true");
    });
    const auto& waterTypeDef = bsr.GetType(waterType);
    for (uint32_t i = 0; i < waterTypeDef.stateCount; ++i) {
        const uint32_t stateId = waterTypeDef.baseStateId + i;
        const bool top = (bsr.GetPropertyValue(stateId, "top") == "true");
        const blockmodels::WaterSpec waterSpec{ top };
        auto model = std::make_shared<const blockstate::BlockModel>(blockmodels::BuildWaterModel(waterSpec));
        modelLibrary.RegisterModel(stateId, std::move(model), bsr);
    }

    // Matches assets/minecraft/blockstates/wheat.json (age 0–7; default full grown).
    bsr.RegisterType("minecraft:wheat", {
        blockstate::BlockProperty{ "age", {"0", "1", "2", "3", "4", "5", "6", "7"}, 7 }
    });
    const uint16_t wheatType = bsr.GetTypeId("minecraft:wheat");
    bsr.SetDerivedData(wheatType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:wheat");

    // Matches assets/minecraft/blockstates/carrots.json (age 0–7; default full grown).
    bsr.RegisterType("minecraft:carrots", {
        blockstate::BlockProperty{ "age", {"0", "1", "2", "3", "4", "5", "6", "7"}, 7 }
    });
    const uint16_t carrotsType = bsr.GetTypeId("minecraft:carrots");
    bsr.SetDerivedData(carrotsType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:carrots");

    // Matches assets/minecraft/blockstates/potatoes.json (age 0–7; default full grown).
    bsr.RegisterType("minecraft:potatoes", {
        blockstate::BlockProperty{ "age", {"0", "1", "2", "3", "4", "5", "6", "7"}, 7 }
    });
    const uint16_t potatoesType = bsr.GetTypeId("minecraft:potatoes");
    bsr.SetDerivedData(potatoesType, [&](blockstate::BlockState& s) {
        s.isRenderable = true;
        s.isTransparent = false;
        s.renderMode = blockstate::RenderMode::Cutout;
        s.isFullBlock = false;
        s.cullSameType = false;
    });
    registerJsonBackedOrLog("minecraft:potatoes");

    // Reuses the pre-existing anvil.png/anvil_top.png/chipped_anvil_top.png/damaged_anvil_top.png
    // textures and blockstate/model JSON (damage 0-2 x facing) that shipped unregistered.
    // Registered last so it doesn't shift type IDs of the blocks above it.
    bsr.RegisterType("minecraft:anvil", {
        blockstate::BlockProperty{ "damage", { "0", "1", "2" } },
        blockstate::BlockProperty{ "facing", { "south", "west", "north", "east" } },
    });
    const uint16_t anvilType = bsr.GetTypeId("minecraft:anvil");
    bsr.SetDerivedData(anvilType, [](blockstate::BlockState& s) {
        s.renderMode = blockstate::RenderMode::Opaque;
        s.isFullBlock = false;
    });
    registerJsonBackedOrLog("minecraft:anvil");

    registerOpaqueJsonBacked("minecraft:emerald_block");

    if (opts.assertUniqueVariantKeys) {
        for (uint16_t tid = 0; tid < bsr.GetTotalTypeCount(); ++tid) {
            blockstate::AssertUniqueVariantKeysPerType(bsr, tid, "RegisterVanillaBlocks");
        }
    }

    if (opts.applyBlockBreakData) {
        blockbreak::ApplyBlockBreakData(bsr);
    }

    if (opts.buildLegacyRemapTable) {
        v1_state_id::BuildRemapTable(bsr);
    }

    ASCIIgL::Logger::Info("BlockStateRegistry: " +
        std::to_string(bsr.GetTotalTypeCount()) + " types, " +
        std::to_string(bsr.GetTotalStateCount()) + " states registered.");
}

void RegisterVanillaBlocksInContext(entt::registry& registry,
                                    const VanillaRegistrationOptions& opts) {
    auto& bsr = registry.ctx().emplace<blockstate::BlockStateRegistry>();
    auto& modelLibrary = registry.ctx().emplace<blockmodels::BlockModelLibrary>();
    RegisterVanillaBlocks(bsr, modelLibrary, opts);
}

} // namespace blockstate
