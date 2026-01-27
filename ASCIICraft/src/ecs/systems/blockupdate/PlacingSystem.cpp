#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

#include <ASCIICraft/ecs/managers/PlayerManager.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>

namespace ecs::systems {
    
    PlacingSystem::PlacingSystem(entt::registry &registry, EventBus& eventBus) noexcept 
        : m_registry(registry)
        , eventBus(eventBus) {

    }

    void PlacingSystem::Update() {
        PlayerPlace();
    }

    void PlacingSystem::PlayerPlace() {
        managers::PlayerManager* pm = managers::GetPlayerPtr(m_registry);
        World* world = GetWorldPtr(m_registry);

        auto& head = m_registry.get<components::Head>(pm->getPlayerEnt());
        glm::vec3 position = pm->GetPosition();
        auto& reach = m_registry.get<components::Reach>(pm->getPlayerEnt());

        const auto& input = ASCIIgL::InputManager::GetInst();

        if (input.IsActionPressed("interact_right")) {
            PlaceBlockEvent event;

            auto rayCast = world->GetChunkManager()->BlockIntersectsViewForPlacement(head.lookDir, head.relativePos + position, reach.reach);
            event.block = Block(BlockType::Bedrock);
            event.position = rayCast.second;

            if (rayCast.first) {
                eventBus.emit(event);
            }
        }
    }
}
