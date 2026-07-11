#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/systems/ISystem.hpp>

namespace ASCIIgL { class EventBus; }

namespace ecs::systems {

class DroppedItemSystem : public ISystem {
public:
    DroppedItemSystem(entt::registry& registry, ASCIIgL::EventBus &eventBus);
    void Update() override;

private:
    entt::registry& registry;
    ASCIIgL::EventBus &eventBus;

    void SpinItems(const float dt);
    void PickupItems(const float dt);
};

} // namespace ecs::systems
