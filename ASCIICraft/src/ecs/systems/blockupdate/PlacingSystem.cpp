#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>
#include <ASCIICraft/world/blockplacement/BlockPlacement.hpp>

namespace ecs::systems {

PlacingSystem::PlacingSystem(entt::registry& registry, EventBus& eventBus)
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

        World* world = GetWorldPtr(m_registry);
        if (!world) break;

        auto& head = m_registry.get<components::Head>(playerEnt);
        auto [position, success] = components::GetPos(playerEnt, m_registry);
        if (!success) break;
        auto& reach = m_registry.get<components::Reach>(playerEnt);

        auto rayCast = world->GetChunkManager()->BlockIntersectsViewForPlacement(head.lookDir, head.relativePos + position, reach.reach);

        if (rayCast.first) {
            auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
            if (!bsr) break;

            // Get the base state (e.g. default grass block)
            uint32_t baseStateId = bsr->GetDefaultState(bsr->GetTypeId("minecraft:bedrock"));
            
            // Apply placement-time logic (grass orientation, etc.) with player placement context
            uint32_t finalizedStateId = ASCIICraft::GetFinalizedBlockStateForPlacement(
                *bsr, baseStateId, rayCast.second, ASCIICraft::PlacementContext::PlayerPlacement
            );

            PlaceBlockEvent placeEvent;
            placeEvent.stateId = finalizedStateId;
            placeEvent.position = rayCast.second;
            m_eventBus.emit(placeEvent);
        }
        break;
    }
}

}
