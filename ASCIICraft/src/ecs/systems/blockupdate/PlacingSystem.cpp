#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

#include <ASCIIgL/engine/InputManager.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

namespace ecs::systems {
    
    PlacingSystem::PlacingSystem(entt::registry &registry, EventBus& eventBus)
        : m_registry(registry)
        , eventBus(eventBus) {}

    void PlacingSystem::Update() {
        PlayerPlace();
    }

    void PlacingSystem::PlayerPlace() {
        entt::entity playerEnt = components::GetPlayerEntity(m_registry);
        if (playerEnt == entt::null) return;
        
        World* world = GetWorldPtr(m_registry);

        auto& head = m_registry.get<components::Head>(playerEnt);
        auto [position, success] = components::GetPos(playerEnt, m_registry);
        if (!success) return;
        auto& reach = m_registry.get<components::Reach>(playerEnt);

        const auto& input = ASCIIgL::InputManager::GetInst();

        if (input.IsActionPressed("interact_right")) {
            auto rayCast = world->GetChunkManager()->BlockIntersectsViewForPlacement(head.lookDir, head.relativePos + position, reach.reach);

            if (rayCast.first) {
                // Get the default state for bedrock from the registry
                auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
                if (!bsr) return;

                PlaceBlockEvent event;
                event.stateId = bsr->GetDefaultState(bsr->GetTypeId("bedrock"));
                event.position = rayCast.second;
                eventBus.emit(event);
            }
        }
    }
}
