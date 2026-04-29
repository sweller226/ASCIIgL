#include <ASCIICraft/ecs/systems/blockupdate/PlacingSystem.hpp>

#include <ASCIICraft/ecs/components/PlayerTag.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Head.hpp>
#include <ASCIICraft/ecs/components/Reach.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/events/PlaceBlockEvent.hpp>
#include <ASCIICraft/world/World.hpp>
#include <ASCIICraft/world/block/state/BlockStateRegistry.hpp>
#include <ASCIICraft/world/block/placement/BlockPlacement.hpp>
#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace ecs::systems {

PlacingSystem::PlacingSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
    : m_registry(registry)
    , m_eventBus(eventBus)
{}

void PlacingSystem::Update() {
    HandleDebugBlockSelection();
    PlayerPlace();
}

void PlacingSystem::HandleDebugBlockSelection() {
    auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
    if (!bsr) return;

    if (!m_selectionInitialized) {
        try {
            m_selectedTypeId = bsr->GetTypeId("minecraft:grass");
        } catch (...) {
            m_selectedTypeId = 0;
        }
        m_selectedStateId = bsr->GetDefaultState(m_selectedTypeId);
        m_selectionInitialized = true;
    }

    auto& input = ASCIIgL::InputManager::GetInst();

    auto clampStateToCurrentType = [&]() {
        const auto& type = bsr->GetType(m_selectedTypeId);
        if (type.stateCount == 0) {
            m_selectedStateId = type.baseStateId;
            return;
        }
        const uint32_t minState = type.baseStateId;
        const uint32_t maxState = type.baseStateId + type.stateCount - 1;
        if (m_selectedStateId < minState) m_selectedStateId = minState;
        if (m_selectedStateId > maxState) m_selectedStateId = maxState;
    };

    bool changed = false;
    const uint16_t maxTypeId = static_cast<uint16_t>(bsr->GetTotalTypeCount() > 0 ? bsr->GetTotalTypeCount() - 1 : 0);

    // 1 / 2: type down/up
    if (input.IsKeyPressed(ASCIIgL::Key::NUM_1) && m_selectedTypeId > 0) {
        --m_selectedTypeId;
        m_selectedStateId = bsr->GetDefaultState(m_selectedTypeId);
        changed = true;
    }
    if (input.IsKeyPressed(ASCIIgL::Key::NUM_2) && m_selectedTypeId < maxTypeId) {
        ++m_selectedTypeId;
        m_selectedStateId = bsr->GetDefaultState(m_selectedTypeId);
        changed = true;
    }

    // 3 / 4: state down/up (bounded to selected type range)
    if (input.IsKeyPressed(ASCIIgL::Key::NUM_3)) {
        clampStateToCurrentType();
        const auto& type = bsr->GetType(m_selectedTypeId);
        const uint32_t minState = type.baseStateId;
        if (m_selectedStateId > minState) {
            --m_selectedStateId;
            changed = true;
        }
    }
    if (input.IsKeyPressed(ASCIIgL::Key::NUM_4)) {
        clampStateToCurrentType();
        const auto& type = bsr->GetType(m_selectedTypeId);
        const uint32_t maxState = type.baseStateId + type.stateCount - 1;
        if (m_selectedStateId < maxState) {
            ++m_selectedStateId;
            changed = true;
        }
    }

    clampStateToCurrentType();

    if (changed) {
        const auto& t = bsr->GetType(m_selectedTypeId);
        ASCIIgL::Logger::Info(
            "Placing selector: typeId=" + std::to_string(m_selectedTypeId) +
            " (" + t.name + "), stateId=" + std::to_string(m_selectedStateId)
        );
    }
}

void PlacingSystem::PlayerPlace() {
    for ([[maybe_unused]] const auto& e : m_eventBus.view<events::SecondaryActionPressedEvent>()) {
        entt::entity playerEnt = components::GetPlayerEntity(m_registry);
        if (playerEnt == entt::null) break;

        World* world = GetWorldPtr(m_registry);
        if (!world) break;

        auto& head = m_registry.get<components::Head>(playerEnt);
        auto [position, success] = components::GetPos(playerEnt, m_registry);
        if (!success) break;
        auto& reach = m_registry.get<components::Reach>(playerEnt);

        auto rayCast = world->GetChunkManager()->BlockIntersectsViewForPlacement(head.lookDir, head.relativePos + position, reach.reach);

        if (rayCast.first) {
            auto* bsr = m_registry.ctx().find<blockstate::BlockStateRegistry>();
            if (!bsr) break;

            // Temporary debug selector (keys 1/2 type, 3/4 state) chooses placed blockstate.
            uint32_t baseStateId = m_selectedStateId;
            if (!bsr->IsValidState(baseStateId)) {
                baseStateId = bsr->GetDefaultState(0);
            }
            
            // Hook for placement-time adjustments (player placement context)
            uint32_t finalizedStateId = blockplacement::FinalizePlacedState(
                *bsr, *world->GetChunkManager(), baseStateId, rayCast.second, blockplacement::PlacementContext::PlayerPlacement
            );

            PlaceBlockEvent placeEvent;
            placeEvent.stateId = finalizedStateId;
            placeEvent.position = rayCast.second;
            m_eventBus.emit(placeEvent);
        }
        break;
    }
}

}
