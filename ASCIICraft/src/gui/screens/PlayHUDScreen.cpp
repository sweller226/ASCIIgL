#include <ASCIICraft/gui/screens/PlayHUDScreen.hpp>
#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/Panel.hpp>

namespace ASCIICraft::gui {

PlayHUDScreen::PlayHUDScreen(entt::registry& registry, EventBus& eventBus, entt::entity playerEntity)
    : m_registry(&registry)
    , m_eventBus(&eventBus)
{
    blocksInput = false;

    constexpr int slots = 9;
    constexpr float slotSize = 32.0f;
    constexpr float slotSpacing = 4.0f;
    constexpr float bottomMargin = 24.0f;
    float hotbarWidth = slots * slotSize + (slots - 1) * slotSpacing;

    anchor = {0.5f, 1.0f};   // bottom center of screen
    offset = {-hotbarWidth / 2.0f, -slotSize - bottomMargin};  // so hotbar top-left is centered and above bottom
    pivot = {0.0f, 0.0f};
    size = {hotbarWidth, slotSize};
    layer = 50;
    SetBackgroundMesh(nullptr);

    for (int i = 0; i < slots; ++i) {
        auto slot = std::make_unique<Slot>(*m_registry, *m_eventBus, playerEntity, i);
        slot->size = {slotSize, slotSize};
        slot->layer = 51;
        slot->pivot = {0.0f, 0.0f};
        slot->offset = {i * (slotSize + slotSpacing), 0.0f};  // from hotbar top-left
        AddChild(std::move(slot));
    }
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

} // namespace ASCIICraft::gui
