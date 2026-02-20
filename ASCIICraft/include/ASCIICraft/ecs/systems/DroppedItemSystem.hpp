#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

// Forward declaration
class EventBus;

namespace ecs::systems {

class DroppedItemSystem : public ISystem {
public:
    DroppedItemSystem(entt::registry& registry, EventBus &eventBus);
    void Update() override;

private:
    entt::registry& registry;
    EventBus &eventBus;

    void DespawnItems(const float dt);
    void SpinItems(const float dt);
    void PickupItems(const float dt);
};

} // namespace ecs::systems
