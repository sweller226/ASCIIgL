#include <ASCIICraft/ecs/factories/gui/GUIFactory.hpp>

#include <ASCIICraft/ecs/components/gui/GUIElement.hpp>
#include <ASCIICraft/ecs/components/gui/GUIPanel.hpp>
#include <ASCIICraft/ecs/components/gui/GUISlot.hpp>
#include <ASCIICraft/ecs/components/gui/GUIButton.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>

#include <vector>

namespace ecs::factories::gui {

using namespace ecs::components;

// Static member initialization
std::shared_ptr<ASCIIgL::Mesh> GUIFactory::s_sharedQuadMesh = nullptr;

void GUIFactory::SetSharedQuadMesh(std::shared_ptr<ASCIIgL::Mesh> mesh) {
    s_sharedQuadMesh = std::move(mesh);
}

std::shared_ptr<ASCIIgL::Mesh> GUIFactory::GetSharedQuadMesh() {
    return s_sharedQuadMesh;
}

entt::entity GUIFactory::Find(entt::registry& registry) const {
    auto view = registry.view<GUIPanel>();
    for (auto entity : view) {
        const auto& panel = view.get<GUIPanel>(entity);
        if (panel.panelId == GetPanelId()) {
            return entity;
        }
    }
    return entt::null;
}

entt::entity GUIFactory::CreateSlotEntity(
    entt::registry& registry,
    entt::entity parentPanel,
    int slotIndex,
    entt::entity inventoryOwner,
    float anchorX, float anchorY,
    float offsetX, float offsetY,
    float slotSize,
    int layer
) {
    entt::entity slot = registry.create();
    
    GUISlot slotComp;
    slotComp.slotIndex = slotIndex;
    slotComp.inventoryOwner = inventoryOwner;
    slotComp.isHovered = false;
    slotComp.isSelected = false;
    slotComp.isDragSource = false;
    registry.emplace<GUISlot>(slot, slotComp);
    
    GUIElement slotElement;
    slotElement.anchor = {anchorX, anchorY};
    slotElement.pivot = {0.5f, 0.5f};
    slotElement.offset = {offsetX, offsetY};
    slotElement.size = {slotSize, slotSize};
    slotElement.layer = layer;
    slotElement.interactable = true;
    slotElement.visible = true;
    slotElement.parentPanel = parentPanel;
    registry.emplace<GUIElement>(slot, slotElement);
    
    registry.emplace<Transform>(slot);
    
    Renderable slotRenderable;
    slotRenderable.renderType = RenderType::ELEM_2D;
    slotRenderable.mesh = s_sharedQuadMesh;
    slotRenderable.layer = layer;
    slotRenderable.visible = true;
    registry.emplace<Renderable>(slot, slotRenderable);
    
    return slot;
}

entt::entity GUIFactory::CreatePanelEntity(
    entt::registry& registry,
    const std::string& panelId,
    bool isOpen,
    bool blocksInput,
    bool closeOnEscape,
    float anchorX, float anchorY,
    float offsetX, float offsetY,
    float width, float height,
    int layer
) {
    entt::entity panel = registry.create();
    
    GUIPanel panelComp;
    panelComp.panelId = panelId;
    panelComp.isOpen = isOpen;
    panelComp.parentPanel = entt::null;
    panelComp.blocksGameInput = blocksInput;
    panelComp.closeOnEscape = closeOnEscape;
    registry.emplace<GUIPanel>(panel, panelComp);
    
    GUIElement panelElement;
    panelElement.anchor = {anchorX, anchorY};
    panelElement.pivot = {0.5f, 0.5f};
    panelElement.offset = {offsetX, offsetY};
    panelElement.size = {width, height};
    panelElement.layer = layer;
    panelElement.interactable = false;
    panelElement.visible = true;
    panelElement.parentPanel = entt::null; // Panels are root level
    registry.emplace<GUIElement>(panel, panelElement);
    
    registry.emplace<Transform>(panel);
    
    Renderable panelRenderable;
    panelRenderable.renderType = RenderType::ELEM_2D;
    panelRenderable.mesh = s_sharedQuadMesh;
    panelRenderable.layer = layer;
    panelRenderable.visible = true;
    registry.emplace<Renderable>(panel, panelRenderable);
    
    return panel;
}

entt::entity GUIFactory::CreateButtonEntity(
    entt::registry& registry,
    entt::entity parentPanel,
    const std::string& actionId,
    const std::string& label,
    float anchorX, float anchorY,
    float offsetX, float offsetY,
    float width, float height,
    int layer
) {
    entt::entity button = registry.create();
    
    GUIButton buttonComp;
    buttonComp.actionId = actionId;
    buttonComp.label = label;
    buttonComp.isHovered = false;
    buttonComp.isSelected = false;
    buttonComp.isDisabled = false;
    registry.emplace<GUIButton>(button, buttonComp);
    
    GUIElement buttonElement;
    buttonElement.anchor = {anchorX, anchorY};
    buttonElement.pivot = {0.5f, 0.5f};
    buttonElement.offset = {offsetX, offsetY};
    buttonElement.size = {width, height};
    buttonElement.layer = layer;
    buttonElement.interactable = true;
    buttonElement.visible = true;
    buttonElement.parentPanel = parentPanel;
    registry.emplace<GUIElement>(button, buttonElement);
    
    registry.emplace<Transform>(button);
    
    Renderable buttonRenderable;
    buttonRenderable.renderType = RenderType::ELEM_2D;
    buttonRenderable.mesh = s_sharedQuadMesh;
    buttonRenderable.layer = layer;
    buttonRenderable.visible = true;
    registry.emplace<Renderable>(button, buttonRenderable);
    
    return button;
}

void GUIFactory::DestroySlots(entt::registry& registry, entt::entity inventoryOwner) {
    auto view = registry.view<GUISlot>();
    std::vector<entt::entity> toDestroy;
    
    for (auto entity : view) {
        const auto& slot = view.get<GUISlot>(entity);
        if (slot.inventoryOwner == inventoryOwner) {
            toDestroy.push_back(entity);
        }
    }
    
    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

void GUIFactory::DestroySlotsInRange(entt::registry& registry, int startIndex, int endIndex) {
    auto view = registry.view<GUISlot>();
    std::vector<entt::entity> toDestroy;
    
    for (auto entity : view) {
        const auto& slot = view.get<GUISlot>(entity);
        if (slot.slotIndex >= startIndex && slot.slotIndex < endIndex) {
            toDestroy.push_back(entity);
        }
    }
    
    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

void GUIFactory::DestroyButtonsWithPrefix(entt::registry& registry, const std::string& prefix) {
    auto view = registry.view<GUIButton>();
    std::vector<entt::entity> toDestroy;
    
    for (auto entity : view) {
        const auto& button = view.get<GUIButton>(entity);
        // Check if actionId starts with prefix
        if (button.actionId.rfind(prefix, 0) == 0) {
            toDestroy.push_back(entity);
        }
    }
    
    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

void GUIFactory::DestroyChildren(entt::registry& registry, entt::entity parentPanel) {
    auto view = registry.view<GUIElement>();
    std::vector<entt::entity> toDestroy;
    
    for (auto entity : view) {
        const auto& element = view.get<GUIElement>(entity);
        if (element.parentPanel == parentPanel) {
            toDestroy.push_back(entity);
        }
    }
    
    for (auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

} // namespace ecs::factories::gui
