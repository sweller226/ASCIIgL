#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>

#include <ASCIICraft/ecs/components/BlockTarget.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/InputEvents.hpp>

namespace ecs::systems {

MiningSystem::MiningSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
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

        const auto* target = m_registry.try_get<components::BlockTarget>(playerEnt);
        if (!target || !target->active) break;

        events::BreakBlockEvent breakEvent;
        breakEvent.stateId = target->stateId;
        breakEvent.position = target->blockPos;
        m_eventBus.emit(breakEvent);
        break;
    }
}

} // namespace ecs::systems
