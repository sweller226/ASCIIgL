#include <ASCIICraft/gui/screens/PlayHUDScreen.hpp>
#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/Panel.hpp>
#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/gui/GUISurface.hpp>
#include <ASCIIgL/engine/TextureLibrary.hpp>

namespace gui {

namespace {

constexpr float kWidgetsAtlasSize = 256.0f;
constexpr float kHotbarBottomMargin = 0.0f;
constexpr float kSlotSize = 20.0f;
constexpr float kSlotInset = 3.0f;

// widgets.png atlas regions (256x256), Minecraft-style pixel coords (origin top-left).
// Future button regions:
//   button.disabled: (0, 46, 200, 20)
//   button.normal:   (0, 66, 200, 20)
//   button.hover:    (0, 86, 200, 20)

constexpr int kHotbarBgX = 0;
constexpr int kHotbarBgY = 0;
constexpr int kHotbarBgW = 182;
constexpr int kHotbarBgH = 22;

constexpr int kHotbarSelectionX = 0;
constexpr int kHotbarSelectionY = 22;
constexpr int kHotbarSelectionW = 24;
constexpr int kHotbarSelectionH = 23;

} // namespace

PlayHUDScreen::PlayHUDScreen(entt::registry& registry,
                             ASCIIgL::EventBus& eventBus,
                             entt::entity playerEntity,
                             GUISurfaceLibrary& surfaceLibrary)
    : m_registry(&registry)
    , m_eventBus(&eventBus)
{
    blocksInput = false;
    name = "guiscreen:playHud";
    layer = 50;

    auto widgetsTexture = ASCIIgL::TextureLibrary::GetInst().GetTexture("widgetsTexture");
    if (widgetsTexture) {
        m_hotbarBgSurface = surfaceLibrary.GetOrCreate("widgets.hotbar.bg", [&]() {
            return GUISurface::FromAtlasRegion(widgetsTexture, kWidgetsAtlasSize, kHotbarBgX, kHotbarBgY, kHotbarBgW, kHotbarBgH);
        });

        m_hotbarSelectionSurface = surfaceLibrary.GetOrCreate("widgets.hotbar.selection", [&]() {
            return GUISurface::FromAtlasRegion(
                widgetsTexture,
                kWidgetsAtlasSize,
                kHotbarSelectionX,
                kHotbarSelectionY,
                kHotbarSelectionW,
                kHotbarSelectionH
            );
        });
    }

    constexpr int slots = 9;
    for (int i = 0; i < slots; ++i) {
        auto slot = std::make_unique<Slot>(*m_registry, *m_eventBus, playerEntity, i);
        slot->size = {kSlotSize, kSlotSize};
        slot->layer = 51;
        slot->pivot = {0.0f, 0.0f};
        AddChild(std::move(slot));
    }
}

void PlayHUDScreen::Layout(glm::vec2 screenSize, const glm::vec2* parentTopLeft) {
    anchor = {0.0f, 0.0f};
    pivot = {0.0f, 0.0f};
    offset = {0.0f, 0.0f};
    size = screenSize;
    Panel::Layout(screenSize, parentTopLeft);

    const glm::vec2 hotbarSize{static_cast<float>(kHotbarBgW), static_cast<float>(kHotbarBgH)};
    const glm::vec2 hotbarAnchor{0.5f, 1.0f};
    const glm::vec2 hotbarPivot{0.5f, 1.0f};
    const glm::vec2 hotbarOffset{0.0f, -kHotbarBottomMargin};
    m_hotbarTopLeft = hotbarAnchor * screenSize - hotbarPivot * hotbarSize + hotbarOffset;

    int slotIndex = 0;
    for (auto& child : GetChildren()) {
        child->offset = {
            m_hotbarTopLeft.x + kSlotInset + static_cast<float>(slotIndex) * kSlotSize,
            m_hotbarTopLeft.y + kSlotInset
        };
        child->Layout(screenSize, &screenPosition);
        ++slotIndex;
    }
}

void PlayHUDScreen::Draw(GUIRenderer& renderer) const {
    if (!visible) return;

    if (m_hotbarBgSurface.mesh && m_hotbarBgSurface.material) {
        renderer.RenderGUIQuad(
            m_hotbarTopLeft,
            {static_cast<float>(kHotbarBgW), static_cast<float>(kHotbarBgH)},
            layer,
            m_hotbarBgSurface.mesh,
            m_hotbarBgSurface.material
        );
    }

    for (const auto& child : GetChildren())
        child->Draw(renderer);
}

bool PlayHUDScreen::OnClick(glm::vec2 position, int button) {
    Widget* w = HitTest(position);
    Slot* s = dynamic_cast<Slot*>(w);
    if (s) {
        s->OnClicked(button, false);
        return true;
    }
    return false;
}

} // namespace gui
