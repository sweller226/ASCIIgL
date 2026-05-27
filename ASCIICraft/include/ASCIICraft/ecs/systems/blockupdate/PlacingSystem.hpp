#pragma once

#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/ecs/systems/ISystem.hpp>

#include <entt/entt.hpp>

namespace ecs::systems {

class PlacingSystem : public ISystem {
public:
    PlacingSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus);

    void Update() override;

private:
    entt::registry& m_registry;
    ASCIIgL::EventBus& m_eventBus;
    uint16_t m_selectedTypeId{0};
    uint32_t m_selectedStateId{0};
    bool m_selectionInitialized{false};

    void HandleDebugBlockSelection();
    void PlayerPlace();
};

}

