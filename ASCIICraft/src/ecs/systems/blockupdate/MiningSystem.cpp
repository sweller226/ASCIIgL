#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

namespace ecs::systems {

MiningSystem::MiningSystem(entt::registry& registry, EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{}

void MiningSystem::Update() {
    CreativeBreakEvents();
}

void MiningSystem::CreativeBreakEvents() {
    for ([[maybe_unused]] const auto& e : m_eventBus.view<events::PrimaryActionPressedEvent>()) {
        entt::entity playerEnt = components::GetPlayerEntity(m_registry);
        if (playerEnt == entt::null) break;

        World* world = GetWorldPtr(m_registry);
        if (!world) break;

        auto& head = m_registry.get<components::Head>(playerEnt);
        auto [position, success] = components::GetPos(playerEnt, m_registry);
        if (!success) break;
        auto& reach = m_registry.get<components::Reach>(playerEnt);

        auto rayCast = world->GetChunkManager()->BlockIntersectsView(head.lookDir, head.relativePos + position, reach.reach);

        if (rayCast.first != blockstate::BlockStateRegistry::AIR_STATE_ID) {
            BreakBlockEvent breakEvent;
            breakEvent.stateId = rayCast.first;
            breakEvent.position = rayCast.second;
            m_eventBus.emit(breakEvent);
        }
        break;
    }
}

}
