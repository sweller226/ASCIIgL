// ecs/components/PlayerCamera.hpp
#pragma once

#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/vec3.hpp>

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

}

