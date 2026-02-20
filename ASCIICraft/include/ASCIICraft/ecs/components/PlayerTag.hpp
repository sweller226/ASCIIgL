#pragma once

#include <entt/entt.hpp>

namespace ecs::components {

struct PlayerTag {};

// Helper to find player entity using PlayerTag query
inline entt::entity GetPlayerEntity(entt::registry& registry) {
    auto view = registry.view<PlayerTag>();
    for (auto entity : view) {
        return entity; // Return first (and only) player
    }
    return entt::null;
}

} // namespace ecs::components