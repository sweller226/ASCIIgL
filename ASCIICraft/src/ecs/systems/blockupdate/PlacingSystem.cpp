#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

#include <ASCIICraft/ecs/components/BlockTarget.hpp>
#include <ASCIICraft/ecs/components/HandSwing.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/HotbarSelection.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemProperties.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/PlayerMode.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>
#include <ASCIICraft/ecs/systems/InventorySystem.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/events/ItemEvents.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>
#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>

#include <optional>

namespace {

/// Horizontal look direction used by placement. BlockPlacement decides whether to
/// keep it (stairs) or invert it (furnace-like blocks that face the player).
std::optional<FaceDir> GetPlayerPlacementFacing(
    entt::registry& registry,
    entt::entity playerEnt
) {
    const auto* head = registry.try_get<ecs::components::Head>(playerEnt);
    if (!head) {
        return std::nullopt;
    }

    return DominantHorizontalFaceDir(head->lookDir);
}

bool PlayerOverlapsPlacementCell(
    entt::registry& registry,
    entt::entity playerEnt,
    const WorldCoord& placementPos
) {
    const auto* transform = registry.try_get<ecs::components::Transform>(playerEnt);
    const auto* collider = registry.try_get<ecs::components::Collider>(playerEnt);
    if (!transform || !collider) {
        return false;
    }

    const glm::vec3 playerCenter = transform->position + collider->localOffset;
    const glm::vec3 playerMin = playerCenter - collider->halfExtents;
    const glm::vec3 playerMax = playerCenter + collider->halfExtents;

    const glm::vec3 blockMin(
        static_cast<float>(placementPos.x),
        static_cast<float>(placementPos.y),
        static_cast<float>(placementPos.z)
    );
    const glm::vec3 blockMax = blockMin + glm::vec3(1.0f);

    return playerMin.x < blockMax.x && playerMax.x > blockMin.x &&
           playerMin.y < blockMax.y && playerMax.y > blockMin.y &&
           playerMin.z < blockMax.z && playerMax.z > blockMin.z;
}

} // namespace

namespace ecs::systems {

PlacingSystem::PlacingSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{}

void PlacingSystem::Update() {
    PlayerPlace();
}

void PlacingSystem::PlayerPlace() {
    for ([[maybe_unused]] const auto& e : m_eventBus.view<events::SecondaryActionPressedEvent>()) {
        entt::entity playerEnt = components::GetPlayerEntity(m_registry);
        if (playerEnt == entt::null) break;

        const auto* target = m_registry.try_get<components::BlockTarget>(playerEnt);
        if (!target || !target->canPlace) break;

        auto* inventory = m_registry.try_get<components::Inventory>(playerEnt);
        const auto* hotbar = m_registry.try_get<components::HotbarSelection>(playerEnt);
        if (!inventory || !hotbar) break;

        const int slotIndex = hotbar->selectedSlot;
        if (slotIndex < 0 || slotIndex >= inventory->capacity) break;

        const components::ItemStack& heldStack = inventory->slots[slotIndex];
        if (heldStack.isEmpty()) break;

        auto* itemRegistry = m_registry.ctx().find<ecs::data::ItemRegistry>();
        if (!itemRegistry) break;

        const entt::entity itemDef = itemRegistry->Resolve(heldStack.itemId);
        if (itemDef == entt::null) break;

        const auto* placeable = m_registry.try_get<components::PlaceableProperty>(itemDef);
        if (!placeable || placeable->typeId < 0) break;

        World* world = GetWorldPtr(m_registry);
        if (!world) break;

        ChunkManager* chunkManager = world->GetChunkManager();
        if (!chunkManager) break;

        auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
        if (!bsr) break;

        uint32_t baseStateId = bsr->GetDefaultState(static_cast<uint16_t>(placeable->typeId));
        if (!bsr->IsValidState(baseStateId)) break;

        const std::optional<FaceDir> faceDir = GetPlayerPlacementFacing(m_registry, playerEnt);
        uint32_t finalizedStateId = blockplacement::FinalizePlacedState(
            *bsr,
            *chunkManager,
            baseStateId,
            target->placePos,
            blockplacement::PlacementContext::PlayerPlacement,
            false,
            faceDir
        );
        if (PlayerOverlapsPlacementCell(m_registry, playerEnt, target->placePos)) break;

        events::PlaceBlockEvent placeEvent;
        placeEvent.stateId = finalizedStateId;
        placeEvent.position = target->placePos;
        m_eventBus.emit(placeEvent);

        // Creative / Spectator: place without consuming (same as MiningSystem break).
        const auto* pmode = m_registry.try_get<components::PlayerMode>(playerEnt);
        const GameMode mode = pmode ? pmode->gamemode : GameMode::Survival;
        if (mode == GameMode::Survival) {
            const components::ItemStack oldStack = heldStack;
            InventorySystem::removeItemAt(*inventory, slotIndex, 1);
            m_eventBus.emit(events::InventoryChangedEvent{
                playerEnt, slotIndex, oldStack, inventory->slots[slotIndex]
            });
        }

        if (auto* swing = m_registry.try_get<components::HandSwing>(playerEnt)) {
            swing->Trigger();
        }
        break;
    }
}

}
