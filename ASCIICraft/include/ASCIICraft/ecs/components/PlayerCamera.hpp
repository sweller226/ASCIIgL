// ecs/components/PlayerCamera.hpp
#pragma once

#include <memory>

#include <ASCIIgL/engine/Camera3D.hpp>

#include <glm/vec3.hpp>

namespace ecs::components {

struct PlayerCamera {
    std::unique_ptr<ASCIIgL::Camera3D> camera{nullptr};

    static constexpr float CAMERA_NEAR_PLANE = 0.1f;
    static constexpr float CAMERA_FAR_PLANE = 1000.0f;
    static constexpr float FOV = 70.0f;                      // Field of view in degrees

    static constexpr float PLAYER_EYE_HEIGHT = 1.62f;        // Blocks from feet to eyes
};

}

