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

// Minecraft inventory.png slot layout (176x166 survival player inventory).
// Hotbar and main inventory are separate regions (gap = player preview / armor between them).
constexpr float kSlotHitSize = 18.0f;  // full cell for hit-test (icons still draw 16x16 centered)
constexpr float kSlotStep = 18.0f;
constexpr float kSlotGridOffsetX = 7.0f;   // subtract from each column
constexpr float kSlotGridOffsetY = -1.0f;  // subtract from each rowY

// inventory.png pixel Y (top-left of each 18x18 cell; +Y = downward on screen).
constexpr float kHotbarSlotY = 142.0f;
constexpr float kMainInvTopY = 84.0f; // topmost main row (slots 27-35); bottom row = +36px (slots 9-17)

glm::vec2 SlotCellOffset(int column, float rowY) {
    return {kSlotGridOffsetX + static_cast<float>(column) * kSlotStep, rowY + kSlotGridOffsetY};
}

void AddInventorySlot(InventoryScreen& screen,
                      entt::registry& registry,
                      ASCIIgL::EventBus& eventBus,
                      entt::entity playerEntity,
                      int slotIndex,
                      int column,
                      float rowY,
                      int widgetLayer) {
    auto slot = std::make_unique<Slot>(registry, eventBus, playerEntity, slotIndex);
    slot->size = {kSlotHitSize, kSlotHitSize};
    slot->layer = widgetLayer;
    slot->pivot = {0.0f, 0.0f};
    slot->offset = SlotCellOffset(column, rowY);
    screen.AddChild(std::move(slot));
}

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
                kInventoryContainerH,
                "guiMaterial.inventory"
            );
        });

        const float panelWidth = static_cast<float>(kInventoryContainerW);
        const float panelHeight = static_cast<float>(kInventoryContainerH);
        size = {panelWidth, panelHeight};
        offset = {-panelWidth * 0.5f, -panelHeight * 0.5f};
    }

    const int slotLayer = layer + 1;

    // Hotbar (ECS slots 0-8) — bottom of inventory.png
    for (int col = 0; col < 9; ++col) {
        AddInventorySlot(*this, *m_registry, *m_eventBus, playerEntity, col, col, kHotbarSlotY, slotLayer);
    }

    // Main inventory (ECS 9-35). Storage row 0 = slots 9-17 at bottom (y=84); row 2 = slots 27-35 at top (y=48).
    // visualRow increases downward (+Y); storageRow counts upward from the hotbar.
    for (int visualRow = 0; visualRow < 3; ++visualRow) {
        const float rowY = kMainInvTopY + static_cast<float>(visualRow) * kSlotStep;
        const int storageRow = 2 - visualRow;
        for (int col = 0; col < 9; ++col) {
            const int slotIndex = 9 + storageRow * 9 + col;
            AddInventorySlot(
                *this, *m_registry, *m_eventBus, playerEntity, slotIndex, col, rowY, slotLayer);
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
    if (!w) {
        return false; // outside panel — GUIManager may drop carried stack
    }
    if (auto* s = dynamic_cast<Slot*>(w)) {
        s->OnClicked(button, false);
    }
    // Panel / crafting area: consume click without dropping
    return true;
}

} // namespace gui
