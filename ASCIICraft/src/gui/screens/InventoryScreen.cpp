#include <ASCIICraft/gui/screens/InventoryScreen.hpp>

#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/gui/GUISurface.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>

namespace gui {

namespace {

constexpr float kInventoryAtlasSize = 256.0f;
constexpr int kInventoryContainerX = 0;
constexpr int kInventoryContainerY = 0;
constexpr int kInventoryContainerW = 176;
constexpr int kInventoryContainerH = 166;

} // namespace

InventoryScreen::InventoryScreen(entt::registry& registry,
                                 ASCIIgL::EventBus& eventBus,
                                 entt::entity playerEntity,
                                 GUISurfaceLibrary& surfaceLibrary)
    : m_registry(&registry)
    , m_eventBus(&eventBus)
{
    blocksInput = true;
    name = "guiscreen:inventory";
    layer = 100;
    pivot = {0.0f, 0.0f};
    anchor = {0.5f, 0.5f};

    auto inventoryTexture = ASCIIgL::TextureLibrary::GetInst().GetTexture("inventoryTexture");
    if (inventoryTexture) {
        m_inventoryPanelSurface = surfaceLibrary.GetOrCreate("inventory.panel", [&]() {
            return GUISurface::FromAtlasRegion(
                inventoryTexture,
                kInventoryAtlasSize,
                kInventoryContainerX,
                kInventoryContainerY,
                kInventoryContainerW,
                kInventoryContainerH
            );
        });

        const float panelWidth = static_cast<float>(kInventoryContainerW);
        const float panelHeight = static_cast<float>(kInventoryContainerH);
        size = {panelWidth, panelHeight};
        offset = {-panelWidth * 0.5f, -panelHeight * 0.5f};
    }

    constexpr int columns = 9;
    constexpr int rows = 4;
    constexpr float slotSize = 32.0f;
    constexpr float slotSpacing = 4.0f;
    constexpr float padding = 16.0f;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            const int slotIndex = row * columns + col;
            auto slot = std::make_unique<Slot>(*m_registry, *m_eventBus, playerEntity, slotIndex);
            slot->size = {slotSize, slotSize};
            slot->layer = layer + 1;
            slot->pivot = {0.0f, 0.0f};
            slot->offset = {
                padding + col * (slotSize + slotSpacing),
                padding + row * (slotSize + slotSpacing)
            };
            AddChild(std::move(slot));
        }
    }
}

void InventoryScreen::Draw(GUIRenderer& renderer) const {
    if (!visible) return;

    if (m_inventoryPanelSurface.mesh && m_inventoryPanelSurface.material) {
        renderer.RenderGUIQuad(
            screenPosition,
            size,
            layer,
            m_inventoryPanelSurface.mesh,
            m_inventoryPanelSurface.material
        );
    }

    for (const auto& child : GetChildren())
        child->Draw(renderer);
}

bool InventoryScreen::TryGetInitialCursorPosition(glm::vec2 screenSize, glm::vec2& out) const {
    const glm::vec2 topLeft = anchor * screenSize - pivot * size + offset;
    out = topLeft + size * 0.5f;
    return true;
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
