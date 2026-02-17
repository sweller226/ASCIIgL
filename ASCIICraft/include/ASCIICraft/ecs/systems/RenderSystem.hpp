// RenderSystem.hpp
#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <unordered_map>

#include <ASCIIgL/engine/Camera2D.hpp>

#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/PlayerCamera.hpp>
#include <glm/vec2.hpp>
#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>

namespace ecs::systems {

class RenderSystem {
public:
    explicit RenderSystem(entt::registry& registry);

    // Main render entry. Call once per frame after camera is set up.
    void Render();
    void SetActive3DCamera(components::PlayerCamera* camera3D);
    void SetActive2DCamera(ASCIIgL::Camera2D* camera2D);

    /// Begin frame: clears draw lists. Call before adding GUI items or collecting ECS items.
    void BeginFrame();
    /// Add a GUI item directly to the 2D draw list. Call during GUI drawing phase (after BeginFrame, before Render).
    /// position/size are in screen pixels (top-left origin). Model matrix is built internally.
    /// material: material to use (must have texture already set). If null, uses materialName lookup.
    void AddGuiItem(glm::vec2 position, glm::vec2 size, int layer,
                    std::shared_ptr<ASCIIgL::Mesh> mesh,
                    std::shared_ptr<ASCIIgL::Material> material = nullptr,
                    const std::string& materialName = {});

private:
    struct DrawItem {
        entt::entity entity;
        std::shared_ptr<ASCIIgL::Mesh> mesh;
        std::shared_ptr<ASCIIgL::Material> material;  // Material to use (if null, falls back to materialName)
        glm::mat4 modelMatrix;
        int32_t layer;
        std::string materialName;  // Fallback: material name if material is null (e.g. "guiItemMaterial")
    };

    entt::registry& m_registry;
    components::PlayerCamera* m_active3DCamera = nullptr;
    ASCIIgL::Camera2D* m_active2DCamera = nullptr;
    std::vector<DrawItem> m_drawList3D;
    std::vector<DrawItem> m_drawList2D;

    void CollectVisible();
    void BatchAndDraw();
};

}