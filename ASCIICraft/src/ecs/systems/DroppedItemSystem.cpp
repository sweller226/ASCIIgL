#include <ASCIICraft/ecs/systems/DroppedItemSystem.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/util/EventBus.hpp>
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

#include <cmath>
#include <vector>

namespace ecs::systems {

static constexpr float kMagnetSpeed = 10.0f; // blocks per second toward player center

DroppedItemSystem::DroppedItemSystem(entt::registry& registry, ASCIIgL::EventBus& eventBus)
    : registry(registry), eventBus(eventBus) {}

void DroppedItemSystem::Update() {
    const float dt = ASCIIgL::FPSClock::GetInst().GetDeltaTime();

    DespawnItems(dt);
    SpinItems(dt);
    PickupItems(dt);
}

void DroppedItemSystem::DespawnItems(const float dt) {
    using namespace ecs::components;

    std::vector<entt::entity> toDestroy;

    auto view = registry.view<DroppedItemTag, Lifetime>();
    for (auto entity : view) {
        auto& lifetime = view.get<Lifetime>(entity);

        lifetime.ageSeconds += dt;

        const bool shouldDespawn = lifetime.shouldDespawn;
        const bool maxReached = (lifetime.maxLifetimeSeconds > 0.0f &&
                                 lifetime.ageSeconds >= lifetime.maxLifetimeSeconds);

        if (shouldDespawn || maxReached) {
            toDestroy.push_back(entity);
        }
    }

    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

void DroppedItemSystem::SpinItems(const float dt) {
    using namespace ecs::components;

    constexpr float SPIN_SPEED = 1.5f;

    auto view = registry.view<DroppedItemTag, Transform>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);

        const float angle = SPIN_SPEED * dt;
        const glm::quat yRotation = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.rotate(yRotation);
    }
}

void DroppedItemSystem::PickupItems(const float dt) {
    using namespace ecs::components;

    auto playerView = registry.view<PlayerTag, Transform, Collider, Inventory>();
    if (playerView.size_hint() == 0) {
        return;
    }

    const entt::entity playerEntity = *playerView.begin();
    const auto& playerTransform = playerView.get<Transform>(playerEntity);
    const auto& playerCollider = playerView.get<Collider>(playerEntity);
    const glm::vec3 playerCenter = playerTransform.position + playerCollider.localOffset;

    auto itemView = registry.view<DroppedItemTag, Transform, Pickup, ItemStack>();
    for (auto entity : itemView) {
        auto& pickup = itemView.get<Pickup>(entity);

        if (pickup.pickupDelay > 0.0f) {
            pickup.pickupDelay -= dt;
            continue;
        }

        auto& itemTransform = itemView.get<Transform>(entity);
        const auto& itemStack = itemView.get<ItemStack>(entity);

        const glm::vec3 toPlayer = playerCenter - itemTransform.position;
        const float distSq = glm::dot(toPlayer, toPlayer);
        const float magnetRadiusSq = pickup.magnetRadius * pickup.magnetRadius;
        const float collectRadiusSq = pickup.collectRadius * pickup.collectRadius;

        if (distSq > magnetRadiusSq) {
            continue;
        }

        if (distSq <= collectRadiusSq) {
            eventBus.emit(events::ItemPickupEvent{
                playerEntity,
                entity,
                itemStack
            });
            continue;
        }

        const float dist = std::sqrt(distSq);
        const glm::vec3 dir = toPlayer / dist;
        const float step = kMagnetSpeed * dt;

        if (dist <= step) {
            itemTransform.setPosition(playerCenter);
        } else {
            itemTransform.translate(dir * step);
        }

        if (auto* vel = registry.try_get<Velocity>(entity)) {
            vel->linear = glm::vec3(0.0f);
        }
    }
}

} // namespace ecs::systems
