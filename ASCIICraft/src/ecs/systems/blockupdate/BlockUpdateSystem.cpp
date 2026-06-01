#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>

// world and chunk
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/placement/FencePlacement.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

namespace {

void RefreshFenceNeighbors(
    const blockstate::BlockStateRegistry& bsr,
    ChunkManager& chunkManager,
    const WorldCoord& changedPos
) {
    for (FaceDir dir : kHorizontalFaceDirs) {
        const WorldCoord neighborPos = NeighborCoord(changedPos, dir);
        const uint32_t neighborStateId = chunkManager.GetBlockState(neighborPos);
        if (!bsr.IsValidState(neighborStateId)) continue;

        const uint16_t neighborTypeId = bsr.GetTypeIdFromState(neighborStateId);
        const auto& neighborType = bsr.GetType(neighborTypeId);
        if (!blockplacement::detail::IsFenceTypeName(neighborType.name)) continue;

        const uint32_t finalizedNeighborStateId =
            blockplacement::detail::FinalizeFencePlacedState(bsr, chunkManager, neighborStateId, neighborPos);
        if (finalizedNeighborStateId != neighborStateId) {
            chunkManager.SetBlockState(neighborPos, finalizedNeighborStateId);
        }
    }
}

} // namespace

namespace ecs::systems {

    BlockUpdateSystem::BlockUpdateSystem(entt::registry &registry, ASCIIgL::EventBus& eventBus) 
        : m_registry(registry)
        , eventBus(eventBus) {}

    void BlockUpdateSystem::Update() {
        BreakBlockEvents();
        PlaceBlockEvents();
    }

    void BlockUpdateSystem::BreakBlockEvents() {
        auto& events = eventBus.view<events::BreakBlockEvent>();
        World* world = GetWorldPtr(m_registry);
        auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
        if (!world || !bsr) return;
        ChunkManager* chunkManager = world->GetChunkManager();
        if (!chunkManager) return;

        for (auto& e : events) {
            if (e.stateId == blockstate::BlockStateRegistry::AIR_STATE_ID) { continue; }

            chunkManager->SetBlockState(e.position, blockstate::BlockStateRegistry::AIR_STATE_ID);
            RefreshFenceNeighbors(*bsr, *chunkManager, e.position);
        }
    }
    
    void BlockUpdateSystem::PlaceBlockEvents() {
        auto& events = eventBus.view<events::PlaceBlockEvent>();
        World* world = GetWorldPtr(m_registry);

        auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
        if (!world || !bsr) return;
        ChunkManager* chunkManager = world->GetChunkManager();
        if (!chunkManager) return;

        for (auto& e : events) {
            if (e.stateId == blockstate::BlockStateRegistry::AIR_STATE_ID) { continue; }

            // Event already contains finalized state (orientation applied in PlacingSystem)
            chunkManager->SetBlockState(e.position, e.stateId);
            RefreshFenceNeighbors(*bsr, *chunkManager, e.position);
        }
    }
}