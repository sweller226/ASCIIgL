// ecs/components/PlayerCamera.hpp
#pragma once

#include <memory>

#include <ASCIIgL/engine/Camera2D.hpp>

#include <glm/vec3.hpp>

namespace ecs::components {
    
struct GUICamera {
    std::shared_ptr<ASCIIgL::Camera2D> camera{nullptr};
    void setCamera(std::shared_ptr<ASCIIgL::Camera2D> cam) {
        camera = std::move(cam); // takes ownership
    }
};

}

