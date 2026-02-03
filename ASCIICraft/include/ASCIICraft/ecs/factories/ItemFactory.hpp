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
    /// Configuration for dropped item behavior
    struct DroppedItemConfig {
        float pickupRadius = 1.5f;      // Distance at which player can pick up
        float pickupDelay = 0.5f;       // Delay before item can be picked up (seconds)
        float lifetimeSeconds = 300.0f; // Time until despawn (5 minutes)
        float spinSpeed = 2.0f;         // Rotation speed (radians/second)
    };

    explicit ItemFactory(entt::registry& registry);

    /// Creates a dropped item entity from raw ItemStack data.
    entt::entity createDroppedItem(
        const ecs::components::ItemStack& itemStack,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f),
        std::shared_ptr<ASCIIgL::Mesh> mesh = nullptr,
        const DroppedItemConfig& config = {}
    );

    /// Creates a dropped item entity by numeric ID.
    /// @return The created entity, or entt::null if item ID not found
    entt::entity createDroppedItemById(
        int itemId,
        int count,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f),
        const DroppedItemConfig& config = {}
    );

    /// Creates a dropped item entity by registry name (e.g., "minecraft:diamond").
    /// @return The created entity, or entt::null if item name not found
    entt::entity createDroppedItemByName(
        const std::string& itemName,
        int count,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f),
        const DroppedItemConfig& config = {}
    );

private:
    entt::registry& registry;
};

} // namespace ecs::factories


