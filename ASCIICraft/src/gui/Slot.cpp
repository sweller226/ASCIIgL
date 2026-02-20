#include <ASCIICraft/gui/Slot.hpp>
#include <ASCIICraft/ecs/systems/RenderSystem.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>
#include <ASCIICraft/ecs/components/ItemVisual.hpp>
#include <ASCIICraft/ecs/data/ItemIndex.hpp>
#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>

namespace ASCIICraft::gui {

Slot::Slot(entt::registry& registry, EventBus& eventBus, entt::entity inventoryOwner, int slotIndex)
    : m_registry(registry)
    , m_eventBus(eventBus)
    , m_inventoryOwner(inventoryOwner)
    , m_slotIndex(slotIndex)
{}

void Slot::Draw(::ecs::systems::RenderSystem& renderSystem) const {
    if (!visible) return;

    // Slots don't have background meshes in current design, but if they did, they'd use a material here
    // if (m_slotBackgroundMesh && m_slotBackgroundMaterial)
    //     renderSystem.AddGuiItem(screenPosition, size, layer, m_slotBackgroundMesh, m_slotBackgroundMaterial);

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
    renderSystem.AddGuiItem(screenPosition, size, layer + 1, visual->mesh, guiItemMat, "guiItemMaterial");
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

} // namespace ASCIICraft::gui
