// RenderSystem.hpp
#pragma once

#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <memory>
#include <unordered_map>

#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <ASCIICraft/ecs/components/GUICamera.hpp>

namespace ecs::systems {

class RenderSystem {
public:
    explicit RenderSystem(entt::registry& registry)
        : m_registry(registry) {}

    // Main render entry. Call once per frame after camera is set up.
    void Render();
    void SetActive3DCamera(components::PlayerCamera* camera3D);
    void SetActive2DCamera(components::GUICamera* camera2D);

private:
    struct DrawItem {
        entt::entity entity;
        std::shared_ptr<ASCIIgL::Mesh> mesh;
        std::shared_ptr<ASCIIgL::Texture> texture; // optional
        glm::mat4 modelMatrix;
        int32_t layer;
    };

    entt::registry& m_registry;
    components::PlayerCamera* m_active3DCamera;
    components::GUICamera* m_active2DCamera;
    std::vector<DrawItem> m_drawList3D;
    std::vector<DrawItem> m_drawList2D; 

    void CollectVisible();
    void BatchAndDraw();
};

}