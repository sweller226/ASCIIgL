#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/placeBlockEvent.hpp>

// world and chunk
#include <ASCIICraft/world/World.hpp>

namespace ecs::systems {

    BlockUpdateSystem::BlockUpdateSystem(entt::registry &registry, EventBus& eventBus) noexcept 
        : m_registry(registry)
        , eventBus(eventBus) {

    }

    void BlockUpdateSystem::Update() {
        BreakBlockEvents();
        PlaceBlockEvents();
    }

    void BlockUpdateSystem::BreakBlockEvents() {
        auto& events = eventBus.view<BreakBlockEvent>();
        World* world = GetWorldPtr(m_registry);

        for (auto& e : events) {
            if (!e.block) { continue; }

            world->GetChunkManager()->SetBlock(e.position, Block(BlockType::Air));
        }
    }
    
    void BlockUpdateSystem::PlaceBlockEvents() {
        auto& events = eventBus.view<PlaceBlockEvent>();
        World* world = GetWorldPtr(m_registry);

        for (auto& e : events) {
            if (e.block.type == BlockType::Air) { continue; }

            world->GetChunkManager()->SetBlock(e.position, e.block);
        }
    }

}