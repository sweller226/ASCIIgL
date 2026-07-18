#include <ASCIICraft/ecs/systems/blockupdate/BlockUpdateSystem.hpp>

// events
#include <ASCIICraft/events/BreakBlockEvent.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>

// world and chunk
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/placement/FencePlacement.hpp>
#include <ASCIICraft/world/block/state/FaceDir.hpp>

#include <ASCIICraft/ecs/data/ItemRegistry.hpp>
#include <ASCIICraft/ecs/factories/ItemFactory.hpp>

#include <glm/vec3.hpp>

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

glm::vec3 BlockDropPosition(const WorldCoord& pos) {
    return glm::vec3(
        static_cast<float>(pos.x) + 0.5f,
        static_cast<float>(pos.y) + 0.5f,
        static_cast<float>(pos.z) + 0.5f
    );
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
        auto* itemRegistry = m_registry.ctx().find<data::ItemRegistry>();
        if (!world || !bsr || !itemRegistry) return;
        ChunkManager* chunkManager = world->GetChunkManager();
        if (!chunkManager) return;

        for (auto& e : events) {
            if (e.stateId == blockstate::BlockStateRegistry::AIR_STATE_ID) { continue; }
            if (!bsr->IsValidState(e.stateId)) { continue; }

            const uint16_t typeId = bsr->GetTypeIdFromState(e.stateId);
            const auto& type = bsr->GetType(typeId);

            chunkManager->SetBlockState(e.position, blockstate::BlockStateRegistry::AIR_STATE_ID);
            RefreshFenceNeighbors(*bsr, *chunkManager, e.position);

            // Mining without a sufficient tool breaks the block but yields no drop.
            if (e.harvested && itemRegistry->Resolve(type.name) != entt::null) {
                factories::ItemFactory itemFactory(m_registry);
                itemFactory.createDroppedItemByName(type.name, 1, BlockDropPosition(e.position));
            }
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