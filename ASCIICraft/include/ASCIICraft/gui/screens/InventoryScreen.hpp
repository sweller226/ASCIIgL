#pragma once

#include <ASCIICraft/gui/Screen.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace ASCIIgL { class Texture; class Mesh; }
namespace ecs::systems { class RenderSystem; }
class EventBus;

namespace ASCIICraft::gui {

/// Blocking inventory screen: 9×4 slot grid. E to toggle (handled by GuiManager).
class InventoryScreen : public Screen {
public:
    /// panelQuad: one full-screen-sized quad; inventoryTexture is the single inventory PNG (Minecraft-style).
    InventoryScreen(entt::registry& registry, EventBus& eventBus, entt::entity playerEntity,
                   std::shared_ptr<ASCIIgL::Mesh> panelQuad,
                   std::shared_ptr<ASCIIgL::Texture> inventoryTexture = nullptr);

    void OnDraw(::ecs::systems::RenderSystem& renderSystem) const override;
    bool OnClick(glm::vec2 position, int button) override;

private:
    entt::registry* m_registry = nullptr;
    EventBus* m_eventBus = nullptr;
    std::shared_ptr<ASCIIgL::Texture> m_inventoryTexture;
};

} // namespace ASCIICraft::gui
