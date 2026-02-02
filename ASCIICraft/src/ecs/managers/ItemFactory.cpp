#pragma once

#include <string>
#include <glm/glm.hpp>
#include <entt/entt.hpp>

// components
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Velocity.hpp>
#include <ASCIICraft/ecs/components/PhysicsBody.hpp>

namespace ecs::factories {

class ItemFactory {
public:

    struct ItemTemplate {
        std::string id;                 // Unique identifier ("minecraft:diamond")
        std::string displayName;        // Human-readable name

        bool stackable = true;          // Can items stack in inventory?
        uint32_t maxStackSize = 64;     // Max stack size

        float pickupRadius = 1.0f;      // How close the player must be to pick it up
        float lifetimeSeconds = 300.0f; // Despawn time (0 = never despawn)

        // Physics properties for dropped items
        bool hasPhysics = true;
        glm::vec3 colliderHalfExtents = glm::vec3(0.25f);
        glm::vec3 colliderOffset = glm::vec3(0.0f);
        uint32_t colliderLayer = 2;
        uint32_t colliderMask = 0xFFFFFFFFu;

        // Optional render data (sprite ID, mesh ID, etc.)
        int meshID = -1;

        // Optional custom metadata (durability, enchantments, etc.)
        // You can replace this with a variant/map later
        uint32_t metadata = 0;
    };

    ItemFactory(entt::registry& registry);

    // Creates a dropped item entity in the world
    entt::entity createItemEnt(const ItemTemplate& item,
                               const glm::vec3& position,
                               const glm::vec3& initialVelocity = glm::vec3(0.0f));

private:
    entt::registry& registry;
};

} // namespace ecs::factories