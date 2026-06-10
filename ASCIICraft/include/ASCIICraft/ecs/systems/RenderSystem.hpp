#pragma once

#include <entt/entt.hpp>
#include <memory>

#include <glm/mat4x4.hpp>

#include <ASCIIgL/engine/Camera2D.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>

namespace ecs::systems {

class RenderSystem {
public:
    explicit RenderSystem(entt::registry& registry);

    void Render();
    void SetActive3DCamera(components::PlayerCamera* camera3D);
    void SetActive2DCamera(ASCIIgL::Camera2D* camera2D);

private:
    entt::registry& m_registry;
    components::PlayerCamera* m_active3DCamera = nullptr;
    ASCIIgL::Camera2D* m_active2DCamera = nullptr;
};

}