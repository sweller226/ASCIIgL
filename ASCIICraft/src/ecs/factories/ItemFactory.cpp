#include <ASCIICraft/ecs/factories/ItemFactory.hpp>

// Components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/DroppedItemTag.hpp>
#include <ASCIICraft/ecs/components/Pickup.hpp>
#include <ASCIICraft/ecs/components/Lifetime.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>

// Data
#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

namespace ecs::factories {

ItemFactory::ItemFactory(entt::registry& registry) : registry(registry) {}

entt::entity ItemFactory::createDroppedItem(
    const ecs::components::ItemStack& itemStack,
    const glm::vec3& position,
    const glm::vec3& velocity,
    std::shared_ptr<ASCIIgL::Mesh> mesh,
    const DroppedItemConfig& config
) {
    using namespace ecs::components;

    entt::entity entity = registry.create();

    // Position and movement
    auto& transform = registry.emplace<Transform>(entity);
    transform.position = position;

    auto& vel = registry.emplace<Velocity>(entity);
    vel.linear = velocity;

    // Physics
    auto& body = registry.emplace<PhysicsBody>(entity);
    body.SetMass(0.5f); // Light item

    auto& collider = registry.emplace<Collider>(entity);
    collider.halfExtents = glm::vec3(0.125f); // Small cube collider
    collider.localOffset = glm::vec3(0.0f);

    registry.emplace<Gravity>(entity);
    registry.emplace<GroundPhysics>(entity);

    // Rendering - try registry mesh if none provided
    std::shared_ptr<ASCIIgL::Mesh> finalMesh = mesh;
    if (!finalMesh) {
        const auto* def = ecs::data::ItemRegistry::Instance().getById(itemStack.itemId);
        if (def && def->droppedMesh) {
            finalMesh = def->droppedMesh;
        }
    }

    if (finalMesh) {
        auto& renderable = registry.emplace<Renderable>(entity);
        renderable.renderType = RenderType::ELEM_3D;
        renderable.SetMesh(finalMesh);
    }

    // Dropped item tag for system queries
    registry.emplace<DroppedItemTag>(entity);

    // Pickup behavior
    auto& pickup = registry.emplace<Pickup>(entity);
    pickup.pickupRadius = config.pickupRadius;
    pickup.pickupDelay = config.pickupDelay;

    // Lifetime for despawning
    auto& lifetime = registry.emplace<Lifetime>(entity);
    lifetime.ageSeconds = 0.0f;
    lifetime.maxLifetimeSeconds = config.lifetimeSeconds;

    // Store the item data on the entity for transfer on pickup
    registry.emplace<ItemStack>(entity, itemStack);

    return entity;
}

entt::entity ItemFactory::createDroppedItemById(
    int itemId,
    int count,
    const glm::vec3& position,
    const glm::vec3& velocity,
    const DroppedItemConfig& config
) {
    // Look up item in registry
    const auto* def = ecs::data::ItemRegistry::Instance().getById(itemId);
    if (!def) {
        return entt::null; // Item not registered
    }

    // Create ItemStack from registry definition
    ecs::components::ItemStack stack = ecs::components::ItemStack::fromRegistry(itemId, count);

    // Use registry mesh
    return createDroppedItem(stack, position, velocity, def->droppedMesh, config);
}

entt::entity ItemFactory::createDroppedItemByName(
    const std::string& itemName,
    int count,
    const glm::vec3& position,
    const glm::vec3& velocity,
    const DroppedItemConfig& config
) {
    // Look up item by name
    const auto* def = ecs::data::ItemRegistry::Instance().getByName(itemName);
    if (!def) {
        return entt::null; // Item not registered
    }

    // Delegate to ID-based creation
    return createDroppedItemById(def->id, count, position, velocity, config);
}

} // namespace ecs::factories

