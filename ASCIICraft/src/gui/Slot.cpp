#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/gui/GUIRenderer.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/data/ItemIndex.hpp>
#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>

#include <ASCIIgL/renderer/Material.hpp>

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

    const auto& stack = inv->slots[m_slotIndex];
    if (stack.isEmpty()) return;

    auto* itemIndex = m_registry.ctx().find<ecs::data::ItemIndex>();
    if (!itemIndex) return;

    entt::entity proto = itemIndex->Resolve(stack.itemId);
    if (proto == entt::null) return;

    auto* visual = m_registry.try_get<ecs::components::ItemVisual>(proto);
    if (!visual || !visual->mesh) return;

    // Item icons use guiItemMaterial (texture array already set)
    auto guiItemMat = ASCIIgL::MaterialLibrary::GetInst().Get("guiItemMaterial");
    renderer.RenderGUIQuad(screenPosition, size, layer + 1, visual->mesh, guiItemMat);
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
