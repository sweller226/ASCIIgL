#pragma once

#include <ASCIICraft/gui/Widget.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace ASCIIgL { class Mesh; class Texture; }
namespace ecs::systems { class RenderSystem; }
class EventBus;

namespace ASCIICraft::gui {

/// ECS-bound slot: reads Inventory + ItemIndex for draw; emits SlotClickedEvent on click.
class Slot : public Widget {
public:
    Slot(entt::registry& registry, EventBus& eventBus, entt::entity inventoryOwner, int slotIndex);

    void SetSlotBackgroundMesh(std::shared_ptr<ASCIIgL::Mesh> mesh) { m_slotBackgroundMesh = std::move(mesh); }

    void Draw(::ecs::systems::RenderSystem& renderSystem) const override;

    /// Called when this slot is clicked (from Screen hit-test). Emits SlotClickedEvent.
    void OnClicked(int mouseButton, bool shift);

    entt::entity GetInventoryOwner() const { return m_inventoryOwner; }
    int GetSlotIndex() const { return m_slotIndex; }

private:
    entt::registry& m_registry;
    EventBus& m_eventBus; 
    entt::entity m_inventoryOwner;
    int m_slotIndex;
    std::shared_ptr<ASCIIgL::Mesh> m_slotBackgroundMesh;
};

} // namespace ASCIICraft::gui
