#include <ASCIICraft/ecs/factories/PlayerFactory.hpp>

#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemCarried.hpp>
#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/BlockTarget.hpp>
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
    auto& input   = registry.emplace<components::PlayerInput>(p_ent);

    // --- Inventory (36 slots: 0-8 hotbar, 9-35 main) ---
    auto& inv = registry.emplace<components::Inventory>(p_ent, 36);
    if (auto* itemRegistry = registry.ctx().find<ecs::data::ItemRegistry>()) {
        const char* itemNames[] = {
            "minecraft:bedrock",
            "minecraft:stone",
            "minecraft:dandelion",
            "minecraft:poppy",
            "minecraft:tall_grass",
            "minecraft:fern",
            "minecraft:fence",
            "minecraft:cobblestone",
            "minecraft:stone_stairs",
            "minecraft:cobblestone_slab",
            "minecraft:dirt",
            "minecraft:grass",
            "minecraft:oak_log",
            "minecraft:oak_planks",
            "minecraft:oak_slab",
            "minecraft:oak_leaves",
            "minecraft:crafting_table",
            "minecraft:bookshelf",
            "minecraft:brick_block",
            "minecraft:furnace",
            "minecraft:glass",
            "minecraft:torch",
            "minecraft:coal",
            "minecraft:iron_ingot",
            "minecraft:gold_ingot",
            "minecraft:stick",
            "minecraft:wooden_sword",
            "minecraft:stone_sword",
            "minecraft:iron_sword",
            "minecraft:wooden_shovel",
            "minecraft:stone_shovel",
            "minecraft:iron_shovel",
            "minecraft:wooden_pickaxe",
            "minecraft:stone_pickaxe",
            "minecraft:iron_pickaxe",
            "minecraft:wooden_axe",
            "minecraft:stone_axe",
            "minecraft:iron_axe",
        };

        const auto seedSlot = [&](int slot, const char* itemName) {
            const int id = itemRegistry->GetIdFromName(itemName, registry);
            if (id < 0 || slot < 0 || slot >= inv.capacity) {
                return;
            }
            components::ItemStack stack = components::ItemStack::fromRegistry(registry, id, 1);
            if (!stack.isEmpty()) {
                stack.count = stack.maxStackSize;
                inv.slots[slot] = stack;
            }
        };

        constexpr int itemCount = static_cast<int>(sizeof(itemNames) / sizeof(itemNames[0]));
        for (int slot = 0; slot < inv.capacity && slot < itemCount; ++slot) {
            seedSlot(slot, itemNames[slot]);
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
    glm::vec3 eyePos = position + glm::vec3(0, cam.PLAYER_EYE_HEIGHT, 0);
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
    head.relativePos = glm::vec3(0, cam.PLAYER_EYE_HEIGHT, 0);

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
        case GameMode::Spectator:
            ctrl.movementState = MovementState::Flying;
            flying.enabled = true;
            col.disabled = true;
            break;
    }
}

}
