#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

// Forward declarations
namespace ASCIIgL { class Mesh; }

namespace ecs::components { struct ItemStack; }

namespace ecs::factories {

/// Factory for creating dropped item entities in the world.
/// Inventory management is handled directly via the Inventory component.
class ItemFactory {
public:
    explicit ItemFactory(entt::registry& registry);

    /// Creates a dropped item entity from raw ItemStack data.
    entt::entity createDroppedItem(
        const ecs::components::ItemStack& itemStack,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f),
        std::shared_ptr<ASCIIgL::Mesh> mesh = nullptr
    );

    /// Creates a dropped item entity by numeric ID.
    /// @return The created entity, or entt::null if item ID not found
    entt::entity createDroppedItemById(
        int itemId,
        int count,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f)
    );

    /// Creates a dropped item entity by registry name (e.g., "minecraft:coal").
    /// @return The created entity, or entt::null if item name not found
    entt::entity createDroppedItemByName(
        const std::string& itemName,
        int count,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f)
    );

private:
    entt::registry& registry;
};

} // namespace ecs::factories

