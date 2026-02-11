#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>

// world and chunk
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/blockstate/BlockStateRegistry.hpp>

namespace ecs::systems {

    BlockUpdateSystem::BlockUpdateSystem(entt::registry &registry, EventBus& eventBus) 
        : m_registry(registry)
        , eventBus(eventBus) {}

    void BlockUpdateSystem::Update() {
        BreakBlockEvents();
        PlaceBlockEvents();
    }

    void BlockUpdateSystem::BreakBlockEvents() {
        auto& events = eventBus.view<BreakBlockEvent>();
        World* world = GetWorldPtr(m_registry);

        for (auto& e : events) {
            if (e.stateId == blockstate::BlockStateRegistry::AIR_STATE_ID) { continue; }

            world->GetChunkManager()->SetBlockState(e.position, blockstate::BlockStateRegistry::AIR_STATE_ID);
        }
    }
    
    void BlockUpdateSystem::PlaceBlockEvents() {
        auto& events = eventBus.view<PlaceBlockEvent>();
        World* world = GetWorldPtr(m_registry);

        for (auto& e : events) {
            if (e.stateId == blockstate::BlockStateRegistry::AIR_STATE_ID) { continue; }

            world->GetChunkManager()->SetBlockState(e.position, e.stateId);
        }
    }
}