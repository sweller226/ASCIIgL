#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/GuiItemIcon.hpp>
#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>

namespace gui {

Slot::Slot(entt::registry& registry, ASCIIgL::EventBus& eventBus, entt::entity inventoryOwner, int slotIndex)
    : m_registry(registry)
    , m_eventBus(eventBus)
    , m_inventoryOwner(inventoryOwner)
    , m_slotIndex(slotIndex)
{}

void Slot::Draw(GUIRenderer& renderer) const {
    if (!visible) return;

    if (m_inventoryOwner == entt::null || !m_registry.valid(m_inventoryOwner)) return;

    auto* inv = m_registry.try_get<ecs::components::Inventory>(m_inventoryOwner);
    if (!inv || m_slotIndex < 0 || m_slotIndex >= static_cast<int>(inv->slots.size())) return;

    DrawItemStackIcon(m_registry, renderer, inv->slots[m_slotIndex], screenPosition, size, layer + 1);
}

void Slot::OnClicked(int mouseButton, bool shift) {
    m_eventBus.emit(events::SlotClickedEvent{
        entt::null,
        m_inventoryOwner,
        m_slotIndex,
        mouseButton,
        shift
    });
}

} // namespace gui
