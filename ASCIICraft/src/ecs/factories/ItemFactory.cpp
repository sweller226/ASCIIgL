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
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/components/Stackable.hpp>
#include <ASCIICraft/ecs/components/ItemId.hpp>

#include <ASCIIgL/renderer/Material.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace ecs::factories {

static constexpr float kDroppedItemIconScale = 0.45f;
static constexpr float kDroppedItemBlockScale = 0.35f;

ItemFactory::ItemFactory(entt::registry& registry) : registry(registry) {}

entt::entity ItemFactory::createDroppedItem(
    const ecs::components::ItemStack& itemStack,
    const glm::vec3& position,
    const glm::vec3& velocity,
    std::shared_ptr<ASCIIgL::Mesh> mesh
) {
    using namespace ecs::components;

    entt::entity entity = registry.create();

    auto& vel = registry.emplace<Velocity>(entity);
    vel.linear = velocity;

    // Resolve prototype visual before transform/physics/render setup
    std::shared_ptr<ASCIIgL::Mesh> resolvedMesh = mesh;
    bool is2DIcon = false;

    auto& itemRegistry = registry.ctx().get<data::ItemRegistry>();
    const entt::entity proto = itemRegistry.Resolve(itemStack.itemId);

    if (!resolvedMesh && proto != entt::null) {
        if (auto* visual = registry.try_get<components::ItemVisual>(proto)) {
            resolvedMesh = visual->droppedMesh ? visual->droppedMesh : visual->mesh;
            is2DIcon = visual->is2DIcon;
        }
    } else if (proto != entt::null) {
        if (auto* visual = registry.try_get<components::ItemVisual>(proto)) {
            is2DIcon = visual->is2DIcon;
        }
    }

    const float droppedScale = is2DIcon ? kDroppedItemIconScale : kDroppedItemBlockScale;

    auto& transform = registry.emplace<Transform>(entity);
    transform.setPosition(position);
    transform.setScale(glm::vec3(droppedScale));

    // Physics — collider feet aligned to mesh bottom (entity origin = visual center)
    constexpr float kColliderHalf = 0.125f;
    // Icon dropped meshes span -0.5..0.5 in Y (ModelUnitsToBlockCentered); blocks ditto after localModel pivot.
    const float meshHalfHeight = 0.5f;

    auto& collider = registry.emplace<Collider>(entity);
    collider.halfExtents = glm::vec3(kColliderHalf);
    collider.localOffset = glm::vec3(
        0.0f,
        -meshHalfHeight * transform.scale.y + kColliderHalf,
        0.0f
    );

    registry.emplace<Gravity>(entity);
    registry.emplace<GroundPhysics>(entity);

    // Rendering
    if (resolvedMesh) {
        const char* materialName = is2DIcon ? "droppedItemMaterial" : "droppedItemBlockMaterial";
        auto material = ASCIIgL::MaterialLibrary::GetInst().Get(materialName);

        auto& renderable = registry.emplace<Renderable>(entity);
        renderable.renderType = RenderType::ELEM_3D;
        renderable.mesh = std::move(resolvedMesh);
        renderable.material = material;
        renderable.backfaceCulling = true;
        if (!is2DIcon) {
            // Block item meshes span 0..1 with origin at a corner; pivot to center for spin.
            renderable.localModel = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));
        }
    }

    // Dropped item tag for system queries
    registry.emplace<DroppedItemTag>(entity);

    registry.emplace<Pickup>(entity);

    auto& lifetime = registry.emplace<Lifetime>(entity);
    lifetime.maxLifetimeSeconds = 300.0f;

    // Store the item data on the entity for transfer on pickup
    registry.emplace<ItemStack>(entity, itemStack);

    return entity;
}

entt::entity ItemFactory::createDroppedItemById(
    int itemId,
    int count,
    const glm::vec3& position,
    const glm::vec3& velocity
) {
    // Look up item in ItemRegistry
    auto& itemRegistry = registry.ctx().get<data::ItemRegistry>();
    auto proto = itemRegistry.Resolve(itemId);
    if (proto == entt::null) {
        return entt::null; // Item not registered
    }

    // Create ItemStack from prototype definition
    auto* stackable = registry.try_get<components::Stackable>(proto);
    components::ItemStack stack;
    stack.itemId = itemId;
    stack.count = count;
    stack.maxStackSize = stackable ? stackable->maxStackSize : 1;

    auto* visual = registry.try_get<components::ItemVisual>(proto);
    std::shared_ptr<ASCIIgL::Mesh> defMesh = nullptr;
    if (visual) {
        defMesh = visual->droppedMesh ? visual->droppedMesh : visual->mesh;
    }
    return createDroppedItem(stack, position, velocity, defMesh);
}

entt::entity ItemFactory::createDroppedItemByName(
    const std::string& itemName,
    int count,
    const glm::vec3& position,
    const glm::vec3& velocity
) {
    // Look up item by name
    auto& itemRegistry = registry.ctx().get<data::ItemRegistry>();
    auto proto = itemRegistry.Resolve(itemName);
    if (proto == entt::null) {
        return entt::null; // Item not registered
    }

    // Get numeric ID from prototype
    auto* itemIdComp = registry.try_get<components::ItemId>(proto);
    if (!itemIdComp) {
        return entt::null;
    }

    // Delegate to ID-based creation
    return createDroppedItemById(itemIdComp->numericId, count, position, velocity);
}

} // namespace ecs::factories

