// ecs/components/PlayerCamera.hpp
#pragma once

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/vec3.hpp>
#include <entt/entt.hpp>

namespace ecs::components {

struct PlayerCamera {
    PlayerCamera()
        : camera(glm::vec3(0, 0, 0), FOV, glm::vec2(0, 0), CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE)
    {}
    
    ASCIIgL::Camera3D camera;

    static constexpr float CAMERA_NEAR_PLANE = 0.1f;
    static constexpr float CAMERA_FAR_PLANE = 1000.0f;
    static constexpr float FOV = 80.0f;                      // Field of view in degrees

    static constexpr float PLAYER_EYE_HEIGHT = 1.62f;        // Blocks from feet to eyes
};

inline ASCIIgL::Camera3D* GetPlayerCamera(entt::entity ent, entt::registry& registry) {
    // Validate player entity
    if (ent == entt::null || !registry.valid(ent)) {
        ASCIIgL::Logger::Error("PlayerFactory::GetPlayerCamera: Player entity is invalid or null.");
        return nullptr;
    }

    // Check if the component exists
    if (!registry.all_of<components::PlayerCamera>(ent)) {
        ASCIIgL::Logger::Error("PlayerFactory::GetPlayerCamera: PlayerCamera component missing on player entity.");
        return nullptr;
    }

    // Retrieve the component safely
    auto* camComp = registry.try_get<components::PlayerCamera>(ent);
    if (!camComp) {
        ASCIIgL::Logger::Error("PlayerFactory::GetPlayerCamera: try_get returned NULL for PlayerCamera.");
        return nullptr;
    }

    return &camComp->camera;
}

}

