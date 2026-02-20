#include <ASCIICraft/ecs/systems/DroppedItemSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/events/ItemEvents.hpp>

#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/DroppedItemTag.hpp>
#include <ASCIICraft/ecs/components/Pickup.hpp>
#include <ASCIICraft/ecs/components/Lifetime.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/PlayerTag.hpp>

#include <glm/gtc/constants.hpp>

namespace ecs::systems {

DroppedItemSystem::DroppedItemSystem(entt::registry& registry, EventBus& eventBus) 
    : registry(registry), eventBus(eventBus) {}

void DroppedItemSystem::Update() {
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    DespawnItems(dt);
    SpinItems(dt);
    PickupItems(dt);
}

void DroppedItemSystem::DespawnItems(const float dt) {
    using namespace ecs::components;

    // Collect entities to destroy (can't destroy while iterating)
    std::vector<entt::entity> toDestroy;

    auto view = registry.view<DroppedItemTag, Lifetime>();
    for (auto entity : view) {
        auto& lifetime = view.get<Lifetime>(entity);

        // Increment age
        lifetime.ageSeconds += dt;

        // Check for forced despawn or max lifetime reached
        bool shouldDespawn = lifetime.shouldDespawn;
        bool maxReached = (lifetime.maxLifetimeSeconds > 0.0f && 
                          lifetime.ageSeconds >= lifetime.maxLifetimeSeconds);

        if (shouldDespawn || maxReached) {
            toDestroy.push_back(entity);
        }
    }

    // Destroy entities
    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

void DroppedItemSystem::SpinItems(const float dt) {
    using namespace ecs::components;

    constexpr float SPIN_SPEED = 2.0f; // radians per second

    auto view = registry.view<DroppedItemTag, Transform>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);

        // Rotate around Y-axis
        float angle = SPIN_SPEED * dt;
        glm::quat yRotation = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.rotate(yRotation);
    }
}

void DroppedItemSystem::PickupItems(const float dt) {
    using namespace ecs::components;

    // Find player entity with inventory
    auto playerView = registry.view<PlayerTag, Transform, Inventory>();
    if (playerView.size_hint() == 0) return;

    // Get first player (assuming single player)
    entt::entity playerEntity = *playerView.begin();
    auto& playerTransform = playerView.get<Transform>(playerEntity);

    auto itemView = registry.view<DroppedItemTag, Transform, Pickup, ItemStack>();
    for (auto entity : itemView) {
        auto& pickup = itemView.get<Pickup>(entity);

        // Decrement pickup delay
        if (pickup.pickupDelay > 0.0f) {
            pickup.pickupDelay -= dt;
            continue;
        }

        auto& itemTransform = itemView.get<Transform>(entity);
        auto& itemStack = itemView.get<ItemStack>(entity);

        // Check distance to player
        glm::vec3 diff = playerTransform.position - itemTransform.position;
        float distSq = glm::dot(diff, diff);
        float radiusSq = pickup.pickupRadius * pickup.pickupRadius;

        if (distSq <= radiusSq) {
            // Emit pickup event - InventorySystem will handle the actual transfer
            eventBus.emit(events::ItemPickupEvent{
                playerEntity,
                entity,
                itemStack
            });
        }
    }
}

} // namespace ecs::systems

