#include <ASCIICraft/ecs/factories/PlayerFactory.hpp>

#include <algorithm>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemCarried.hpp>
#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/BlockTarget.hpp>
#include <ASCIICraft/ecs/components/MiningProgress.hpp>
#include <ASCIICraft/ecs/components/HandSwing.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/ViewBobbing.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace ecs::factories {

PlayerFactory::PlayerFactory(entt::registry& registry) 
    : registry(registry) {}

void PlayerFactory::createPlayerEnt(const glm::vec3& position, GameMode mode) {
    // Create entity
    entt::entity p_ent = registry.create();
    
    // --- Core components ---
    auto& t       = registry.emplace<components::Transform>(p_ent);
    registry.emplace<components::PlayerTag>(p_ent);  // Tag component (empty struct)
    auto& vel     = registry.emplace<components::Velocity>(p_ent);
    auto& grav    = registry.emplace<components::Gravity>(p_ent);
    auto& step    = registry.emplace<components::StepPhysics>(p_ent);
    auto& ground  = registry.emplace<components::GroundPhysics>(p_ent);
    auto& flying  = registry.emplace<components::FlyingPhysics>(p_ent);
    auto& ctrl    = registry.emplace<components::PlayerController>(p_ent);
    auto& stepSoundState = registry.emplace<components::StepSoundState>(p_ent);
    auto& viewBobbing    = registry.emplace<components::ViewBobbing>(p_ent);
    auto& jump    = registry.emplace<components::Jump>(p_ent);
    auto& cam     = registry.emplace<components::PlayerCamera>(p_ent);
    auto& pmode   = registry.emplace<components::PlayerMode>(p_ent);
    auto& col     = registry.emplace<components::Collider>(p_ent);
    auto& head    = registry.emplace<components::Head>(p_ent);
    auto& reach   = registry.emplace<components::Reach>(p_ent);
    registry.emplace<components::BlockTarget>(p_ent);
    registry.emplace<components::MiningProgress>(p_ent);
    registry.emplace<components::HandSwing>(p_ent);
    auto& input   = registry.emplace<components::PlayerInput>(p_ent);

    // --- Inventory (36 slots: 0-8 hotbar, 9-35 main) ---
    auto& inv = registry.emplace<components::Inventory>(p_ent, 36);
    if (auto* itemRegistry = registry.ctx().find<ecs::data::ItemRegistry>()) {
        const char* hotbarItems[] = {
            "minecraft:wooden_sword",
            "minecraft:wooden_pickaxe",
            "minecraft:wooden_axe",
            "minecraft:wooden_shovel",
            "minecraft:bread",
            "minecraft:oak_planks",
        };
        const int hotbarCounts[] = {1, 1, 1, 1, 64, 64};

        const char* blockItems[] = {
            "minecraft:dandelion",
            "minecraft:poppy",
            "minecraft:tall_grass",
            "minecraft:fern",
            "minecraft:fence",
            "minecraft:oak_stairs",
            "minecraft:cobblestone",
            "minecraft:stone",
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
            "minecraft:farmland",
        };

        const auto seedSlot = [&](int slot, const char* itemName, int count = -1) {
            const int id = itemRegistry->GetIdFromName(itemName, registry);
            if (id < 0 || slot < 0 || slot >= inv.capacity) {
                return;
            }
            components::ItemStack stack = components::ItemStack::fromRegistry(registry, id, 1);
            if (!stack.isEmpty()) {
                stack.count = (count < 0) ? stack.maxStackSize : std::min(count, stack.maxStackSize);
                inv.slots[slot] = stack;
            }
        };

        constexpr int hotbarItemCount = static_cast<int>(sizeof(hotbarItems) / sizeof(hotbarItems[0]));
        for (int i = 0; i < hotbarItemCount; ++i) {
            seedSlot(i, hotbarItems[i], hotbarCounts[i]);
        }

        constexpr int mainInventoryStart = 9;
        constexpr int blockItemCount = static_cast<int>(sizeof(blockItems) / sizeof(blockItems[0]));
        for (int i = 0; i < blockItemCount; ++i) {
            const int slot = mainInventoryStart + i;
            if (slot >= inv.capacity) {
                break;
            }
            seedSlot(slot, blockItems[i]);
        }
    } else {
        ASCIIgL::Logger::Warning("PlayerFactory: ItemRegistry missing; inventory left empty.");
    }
    registry.emplace<components::ItemCarried>(p_ent);
    registry.emplace<components::HotbarSelection>(p_ent);

    // --- Transform ---
    t.setPosition(position);

    // --- Velocity ---
    vel.maxSpeed = 100.0f;

    // --- Step sound state ---
    stepSoundState.distanceAccum = 0.0f;
    stepSoundState.cooldown = 0.0f;
    stepSoundState.lastPosition = position;

    // --- Camera ---
    glm::vec3 eyePos = position + glm::vec3(0, cam.playerEyeHeight, 0);
    glm::vec3 lookDir = glm::vec3(0.0f, 0.0f, -1.0f);
    const unsigned screenW = ASCIIgL::Screen::GetInst().GetWidth();
    const unsigned screenH = ASCIIgL::Screen::GetInst().GetHeight();
    if (screenW > 0u && screenH > 0u) {
        cam.camera.setScreenDimensions(screenW, screenH);
    }
    cam.camera.setCamPos(eyePos);
    cam.camera.setCamDir(lookDir);

    // --- Head ---
    head.lookDir = lookDir;
    head.relativePos = glm::vec3(0, cam.playerEyeHeight, 0);

    // --- Reach ---
    reach.reach = 5.0f;

    // --- Collider defaults ---
    col.halfExtents = DEFAULT_COLLIDER_HALFEXTENTS;
    col.localOffset = DEFAULT_COLLIDER_OFFSET;
    col.layer       = DEFAULT_COLLIDER_LAYER;
    col.mask        = DEFAULT_COLLIDER_MASK;
    col.disabled    = DEFAULT_COLLIDER_DISABLED;

    // --- Gravity ---
    grav.acceleration = DEFAULT_GRAVITY;
    jump.RefreshJumpVelocity(grav.acceleration.y);
    jump.coyoteTime = jump.COYOTE_TIME_MAX;

    // --- Player mode ---
    pmode.gamemode = mode;

    // --- Movement state based on mode ---
    switch (mode) {
        case GameMode::Survival:
            ctrl.movementState = MovementState::Walking;
            flying.enabled = false;
            col.disabled = false;
            break;
        case GameMode::Creative:
            // Creative starts grounded; double-tap Space toggles flight in MovementSystem.
            ctrl.movementState = MovementState::Walking;
            flying.enabled = false;
            col.disabled = false;
            break;
        case GameMode::Spectator:
            ctrl.movementState = MovementState::Flying;
            flying.enabled = true;
            col.disabled = true;
            break;
    }
}

}
