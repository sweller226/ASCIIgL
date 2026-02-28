#include <ASCIICraft/gui/screens/InventoryScreen.hpp>
#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/Panel.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>

namespace gui {

InventoryScreen::InventoryScreen(entt::registry& registry, EventBus& eventBus, entt::entity playerEntity,
                                  std::shared_ptr<ASCIIgL::Mesh> panelQuad,
                                  std::shared_ptr<ASCIIgL::Texture> inventoryTexture)
    : m_registry(&registry)
    , m_eventBus(&eventBus)
    , m_inventoryTexture(std::move(inventoryTexture))
{
    blocksInput = true;

    constexpr int columns = 9;
    constexpr int rows = 4;
    constexpr float slotSize = 32.0f;
    constexpr float slotSpacing = 4.0f;
    constexpr float padding = 16.0f;

    float panelWidth = padding * 2 + columns * slotSize + (columns - 1) * slotSpacing;
    float panelHeight = padding * 2 + rows * slotSize + (rows - 1) * slotSpacing;

    size = {panelWidth, panelHeight};
    anchor = {0.5f, 0.5f};
    offset = {-panelWidth / 2.0f, -panelHeight / 2.0f};  // center panel: pivot (0,0) so top-left = screen center + offset
    pivot = {0.0f, 0.0f};
    layer = 100;
    SetBackgroundMesh(panelQuad);
    
    // Create material for this panel's texture (cached by MaterialLibrary)
    if (m_inventoryTexture) {
        auto material = ASCIIgL::MaterialLibrary::GetInst().GetOrCreateFromTemplate("guiMaterial", m_inventoryTexture.get());
        SetBackgroundMaterial(material);
    }

    // Slots laid out from panel top-left: offset = (padding + col*..., padding + row*...)
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            int slotIndex = row * columns + col;
            auto slot = std::make_unique<Slot>(*m_registry, *m_eventBus, playerEntity, slotIndex);
            slot->size = {slotSize, slotSize};
            slot->layer = 101;
            slot->pivot = {0.0f, 0.0f};
            slot->offset = {
                padding + col * (slotSize + slotSpacing),
                padding + row * (slotSize + slotSpacing)
            };
            AddChild(std::move(slot));
        }
    }
}

void InventoryScreen::OnDraw(::ecs::systems::RenderSystem& renderSystem) const {
    Panel::Draw(renderSystem);
}

bool InventoryScreen::OnClick(glm::vec2 position, int button) {
    Widget* w = HitTest(position);
    Slot* s = dynamic_cast<Slot*>(w);
    if (s) {
        s->OnClicked(button, false);
        return true;
    }
    return false;
}

} // namespace gui
