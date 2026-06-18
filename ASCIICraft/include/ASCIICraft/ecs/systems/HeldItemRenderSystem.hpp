#pragma once

#include <entt/entt.hpp>

namespace ASCIIgL { class Camera3D; }

namespace ecs::systems {

/// First-person held-item viewmodel pass (camera-attached, not world entities).
class HeldItemRenderSystem {
public:
    explicit HeldItemRenderSystem(entt::registry& registry);

    void Render();
    void SetViewModelCamera(ASCIIgL::Camera3D* camera3D);

private:
    entt::registry& m_registry;
    ASCIIgL::Camera3D* m_viewModelCamera = nullptr;
};

}
