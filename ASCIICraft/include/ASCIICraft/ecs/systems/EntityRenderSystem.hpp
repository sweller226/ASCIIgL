#pragma once

#include <entt/entt.hpp>

#include <ASCIICraft/ecs/components/PlayerCamera.hpp>

namespace ecs::systems {

class EntityRenderSystem {
public:
    explicit EntityRenderSystem(entt::registry& registry);

    void Render();
    void SetActive3DCamera(components::PlayerCamera* camera3D);

private:
    entt::registry& m_registry;
    components::PlayerCamera* m_active3DCamera = nullptr;
};

}
