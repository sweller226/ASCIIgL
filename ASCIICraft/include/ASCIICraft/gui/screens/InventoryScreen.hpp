#pragma once

#include <ASCIICraft/gui/GUIScreen.hpp>
#include <ASCIICraft/gui/GUISurfaceLibrary.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace ASCIIgL { class Texture; }
namespace ecs::systems { class RenderSystem; }
namespace ASCIIgL { class EventBus; }

namespace gui {

/// Blocking inventory screen: 9×4 slot grid. E to toggle (handled by GUIManager).
class InventoryScreen : public GUIScreen {
public:
    /// inventoryTexture is the single inventory PNG (Minecraft-style).
    InventoryScreen(entt::registry& registry, ASCIIgL::EventBus& eventBus, entt::entity playerEntity,
                   GUISurfaceLibrary& meshLibrary,
                   std::shared_ptr<ASCIIgL::Texture> inventoryTexture = nullptr);

    void OnDraw(::ecs::systems::RenderSystem& ecsRenderSystem) const override;
    bool OnClick(glm::vec2 position, int button) override;

private:
    entt::registry* m_registry = nullptr;
    ASCIIgL::EventBus* m_eventBus = nullptr;
    std::shared_ptr<ASCIIgL::Texture> m_inventoryTexture;
};

} // namespace gui
