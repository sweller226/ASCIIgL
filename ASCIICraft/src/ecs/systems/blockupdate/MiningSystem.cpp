#include <ASCIICraft/ecs/systems/blockupdate/MiningSystem.hpp>

#include <ASCIICraft/ecs/managers/PlayerManager.hpp>
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>

namespace ecs::systems {
    
    MiningSystem::MiningSystem(entt::registry &registry, EventBus& eventBus) noexcept 
        : m_registry(registry)
        , eventBus(eventBus) {

    }

    void MiningSystem::Update() {
        CreativeBreakEvents();
    }

    void MiningSystem::CreativeBreakEvents() {
        managers::PlayerManager* pm = managers::GetPlayerPtr(m_registry);
        World* world = GetWorldPtr(m_registry);

        auto& head = m_registry.get<components::Head>(pm->getPlayerEnt());
        glm::vec3 position = pm->GetPosition();
        auto& reach = m_registry.get<components::Reach>(pm->getPlayerEnt());

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
