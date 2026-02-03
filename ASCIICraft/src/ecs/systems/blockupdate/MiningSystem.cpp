#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>

#include <ASCIIgL/engine/InputManager.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>

namespace ecs::systems {
    
    MiningSystem::MiningSystem(entt::registry &registry, EventBus& eventBus) 
        : m_registry(registry)
        , eventBus(eventBus) {

    }

    void MiningSystem::Update() {
        CreativeBreakEvents();
    }

    void MiningSystem::CreativeBreakEvents() {
        entt::entity playerEnt = components::GetPlayerEntity(m_registry);
        if (playerEnt == entt::null) return;
        
        World* world = GetWorldPtr(m_registry);

        auto& head = m_registry.get<components::Head>(playerEnt);
        auto [position, success] = components::GetPos(playerEnt, m_registry);
        if (!success) return;
        auto& reach = m_registry.get<components::Reach>(playerEnt);

        const auto& input = ASCIIgL::InputManager::GetInst();

        if (input.IsActionPressed("interact_left")) {
            BreakBlockEvent event;

            auto rayCast = world->GetChunkManager()->BlockIntersectsView(head.lookDir, head.relativePos + position, reach.reach);
            event.block = rayCast.first;
            event.position = rayCast.second;

            if (event.block) {
                eventBus.emit(event);
            }
        }
    }
}
