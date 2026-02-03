#include <ASCIICraft/ecs/systems/GUISystem.hpp>

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>

#include <ASCIICraft/ecs/components/gui/GUIElement.hpp>
#include <ASCIICraft/ecs/components/gui/GUIPanel.hpp>
#include <ASCIICraft/ecs/components/gui/GUISlot.hpp>
#include <ASCIICraft/ecs/components/gui/GUIButton.hpp>
#include <ASCIICraft/ecs/components/gui/GUICursor.hpp>
#include <ASCIICraft/ecs/components/Transform.hpp>
#include <ASCIICraft/ecs/components/Renderable.hpp>
#include <ASCIICraft/ecs/components/Inventory.hpp>

#include <ASCIICraft/ecs/data/ItemRegistry.hpp>

#include <ASCIIgL/engine/InputManager.hpp>

#include <algorithm>

namespace ecs::systems {

using namespace ecs::components;

GUISystem::GUISystem(entt::registry& registry, EventBus& eventBus)
    : registry(registry), eventBus(eventBus) {}

entt::entity GUISystem::CreateCursor() {
    cursorEntity = registry.create();
    
    GUICursor cursor;
    cursor.position = screenSize * 0.5f;
    cursor.size = {4.0f, 4.0f};
    cursor.moveSpeed = 8.0f;
    cursor.visible = true;
    cursor.hoveredElement = entt::null;
    cursor.previousHovered = entt::null;
    registry.emplace<GUICursor>(cursorEntity, cursor);
    
    // Cursor also needs Transform and Renderable for display
    Transform cursorTransform;
    cursorTransform.position = {cursor.position.x, cursor.position.y, 1000.0f};
    registry.emplace<Transform>(cursorEntity, cursorTransform);
    
    Renderable cursorRenderable;
    cursorRenderable.renderType = RenderType::ELEM_2D;
    cursorRenderable.mesh = nullptr; // Set by user with a cursor mesh
    cursorRenderable.layer = 1000;
    cursorRenderable.visible = true;
    registry.emplace<Renderable>(cursorEntity, cursorRenderable);
    
    return cursorEntity;
}

void GUISystem::Update() {
    UpdateLayout();
    
    // Only process cursor input when a panel is open
    if (IsBlockingInput() && cursorEntity != entt::null) {
        ProcessCursorInput();
        UpdateHoveredElement();
    }
    
    SyncInventorySlots();
}

void GUISystem::SetScreenSize(float width, float height) {
    screenSize = glm::vec2(width, height);
}

void GUISystem::ProcessCursorInput() {
    if (!registry.valid(cursorEntity)) return;
    
    auto& cursor = registry.get<GUICursor>(cursorEntity);
    auto& input = ASCIIgL::InputManager::GetInst();
    
    // Arrow key cursor movement
    if (input.IsKeyHeld(ASCIIgL::Key::LEFT)) {
        cursor.position.x -= cursor.moveSpeed;
    }
    if (input.IsKeyHeld(ASCIIgL::Key::RIGHT)) {
        cursor.position.x += cursor.moveSpeed;
    }
    if (input.IsKeyHeld(ASCIIgL::Key::UP)) {
        cursor.position.y -= cursor.moveSpeed;
    }
    if (input.IsKeyHeld(ASCIIgL::Key::DOWN)) {
        cursor.position.y += cursor.moveSpeed;
    }
    
    // Clamp cursor to screen bounds
    cursor.position.x = std::clamp(cursor.position.x, 0.0f, screenSize.x);
    cursor.position.y = std::clamp(cursor.position.y, 0.0f, screenSize.y);
    
    // Update cursor Transform for rendering
    auto* cursorTransform = registry.try_get<Transform>(cursorEntity);
    if (cursorTransform) {
        cursorTransform->position.x = cursor.position.x;
        cursorTransform->position.y = cursor.position.y;
    }
    
    // Interact actions
    if (input.IsActionPressed("interact_left")) {
        if (cursor.hoveredElement != entt::null) {
            // Check if it's a slot
            auto* slot = registry.try_get<GUISlot>(cursor.hoveredElement);
            if (slot) {
                eventBus.emit(events::SlotClickedEvent{
                    cursor.hoveredElement,
                    slot->slotIndex,
                    0 // Left/primary action
                });
            }
            
            // Check if it's a button
            auto* button = registry.try_get<GUIButton>(cursor.hoveredElement);
            if (button && !button->isDisabled) {
                eventBus.emit(events::ButtonClickedEvent{
                    cursor.hoveredElement,
                    button->actionId,
                    0 // Left/primary action
                });
            }
        }
    }
    
    if (input.IsActionPressed("interact_right")) {
        if (cursor.hoveredElement != entt::null) {
            // Check if it's a slot
            auto* slot = registry.try_get<GUISlot>(cursor.hoveredElement);
            if (slot) {
                eventBus.emit(events::SlotClickedEvent{
                    cursor.hoveredElement,
                    slot->slotIndex,
                    1 // Right/secondary action
                });
            }
            
            // Check if it's a button
            auto* button = registry.try_get<GUIButton>(cursor.hoveredElement);
            if (button && !button->isDisabled) {
                eventBus.emit(events::ButtonClickedEvent{
                    cursor.hoveredElement,
                    button->actionId,
                    1 // Right/secondary action
                });
            }
        }
    }
    
    // ESC to close panels
    if (input.IsKeyPressed(ASCIIgL::Key::ESCAPE)) {
        auto view = registry.view<GUIPanel>();
        for (auto entity : view) {
            auto& panel = view.get<GUIPanel>(entity);
            if (panel.isOpen && panel.closeOnEscape) {
                ClosePanel(panel.panelId);
                break;
            }
        }
    }
}

void GUISystem::UpdateHoveredElement() {
    if (!registry.valid(cursorEntity)) return;
    
    auto& cursor = registry.get<GUICursor>(cursorEntity);
    cursor.previousHovered = cursor.hoveredElement;
    cursor.hoveredElement = FindElementUnderCursor();
    
    // Handle hover state changes
    if (cursor.previousHovered != cursor.hoveredElement) {
        // Clear old hover
        if (cursor.previousHovered != entt::null && registry.valid(cursor.previousHovered)) {
            auto* oldSlot = registry.try_get<GUISlot>(cursor.previousHovered);
            if (oldSlot) {
                oldSlot->isHovered = false;
                oldSlot->isSelected = false;
                eventBus.emit(events::SlotUnhoveredEvent{cursor.previousHovered, oldSlot->slotIndex});
            }
            auto* oldButton = registry.try_get<GUIButton>(cursor.previousHovered);
            if (oldButton) {
                oldButton->isHovered = false;
                oldButton->isSelected = false;
            }
        }
        
        // Set new hover
        if (cursor.hoveredElement != entt::null) {
            auto* newSlot = registry.try_get<GUISlot>(cursor.hoveredElement);
            if (newSlot) {
                newSlot->isHovered = true;
                newSlot->isSelected = true;
                eventBus.emit(events::SlotHoveredEvent{cursor.hoveredElement, newSlot->slotIndex});
            }
            auto* newButton = registry.try_get<GUIButton>(cursor.hoveredElement);
            if (newButton) {
                newButton->isHovered = true;
                newButton->isSelected = true;
                eventBus.emit(events::ButtonHoveredEvent{cursor.hoveredElement, newButton->actionId});
            }
        }
    }
}

entt::entity GUISystem::FindElementUnderCursor() {
    if (!registry.valid(cursorEntity)) return entt::null;
    
    const auto& cursor = registry.get<GUICursor>(cursorEntity);
    entt::entity topmost = entt::null;
    int highestLayer = -1;
    
    auto view = registry.view<GUIElement>();
    for (auto entity : view) {
        const auto& element = view.get<GUIElement>(entity);
        
        // Skip invisible or non-interactable elements
        if (!element.visible || !element.interactable) continue;
        
        // Check if parent panel is open (if has parent)
        if (element.parentPanel != entt::null) {
            auto* parentPanel = registry.try_get<GUIPanel>(element.parentPanel);
            if (parentPanel && !parentPanel->isOpen) continue;
        }
        
        // Check AABB collision
        if (CursorIntersectsElement(cursor.position, cursor.size, 
                                     element.screenPosition, element.size)) {
            // Take highest layer element
            if (element.layer > highestLayer) {
                highestLayer = element.layer;
                topmost = entity;
            }
        }
    }
    
    return topmost;
}

bool GUISystem::CursorIntersectsElement(
    const glm::vec2& cursorPos,
    const glm::vec2& cursorSize,
    const glm::vec2& elementPos, 
    const glm::vec2& elementSize
) const {
    // Element bounds (assuming pivot is center)
    float elemLeft = elementPos.x - elementSize.x * 0.5f;
    float elemRight = elementPos.x + elementSize.x * 0.5f;
    float elemTop = elementPos.y - elementSize.y * 0.5f;
    float elemBottom = elementPos.y + elementSize.y * 0.5f;
    
    // Cursor bounds
    float curLeft = cursorPos.x - cursorSize.x * 0.5f;
    float curRight = cursorPos.x + cursorSize.x * 0.5f;
    float curTop = cursorPos.y - cursorSize.y * 0.5f;
    float curBottom = cursorPos.y + cursorSize.y * 0.5f;
    
    // AABB intersection test
    return !(curRight < elemLeft || curLeft > elemRight ||
             curBottom < elemTop || curTop > elemBottom);
}

void GUISystem::UpdateLayout() {
    auto view = registry.view<GUIElement>();
    
    for (auto entity : view) {
        auto& element = view.get<GUIElement>(entity);
        
        // Check parent visibility
        if (element.parentPanel != entt::null) {
            auto* parentPanel = registry.try_get<GUIPanel>(element.parentPanel);
            if (parentPanel && !parentPanel->isOpen) {
                // Hide elements of closed panels
                auto* renderable = registry.try_get<Renderable>(entity);
                if (renderable) renderable->visible = false;
                continue;
            }
        }
        
        if (!element.visible) continue;
        
        element.screenPosition = CalculateScreenPosition(
            element.anchor,
            element.pivot,
            element.offset,
            element.size
        );
        
        // Update Transform and Renderable
        auto* transform = registry.try_get<Transform>(entity);
        if (transform) {
            transform->position.x = element.screenPosition.x;
            transform->position.y = element.screenPosition.y;
            transform->position.z = static_cast<float>(element.layer);
        }
        
        auto* renderable = registry.try_get<Renderable>(entity);
        if (renderable) {
            renderable->visible = element.visible;
            renderable->layer = element.layer;
        }
    }
}

void GUISystem::SyncInventorySlots() {
    auto view = registry.view<GUISlot, Renderable>();
    
    for (auto entity : view) {
        auto& slot = view.get<GUISlot>(entity);
        auto& renderable = view.get<Renderable>(entity);
        
        if (slot.inventoryOwner == entt::null || !registry.valid(slot.inventoryOwner)) {
            continue;
        }
        
        auto* inventory = registry.try_get<Inventory>(slot.inventoryOwner);
        if (!inventory || slot.slotIndex < 0 || slot.slotIndex >= inventory->capacity) {
            continue;
        }
        
        const ItemStack& itemStack = inventory->slots[slot.slotIndex];
        
        if (itemStack.isEmpty()) {
            // Could set to an empty slot mesh
        } else {
            // Could set to item icon mesh from registry
            const auto* def = ecs::data::ItemRegistry::Instance().getById(itemStack.itemId);
            if (def) {
                // renderable.mesh = def->iconMesh; // Would need icon mesh in ItemDefinition
            }
        }
    }
}

glm::vec2 GUISystem::CalculateScreenPosition(
    const glm::vec2& anchor,
    const glm::vec2& pivot,
    const glm::vec2& offset,
    const glm::vec2& size
) const {
    glm::vec2 anchorPos = anchor * screenSize;
    glm::vec2 pivotOffset = pivot * size;
    return anchorPos - pivotOffset + offset;
}

void GUISystem::OpenPanel(const std::string& panelId) {
    auto view = registry.view<GUIPanel>();
    for (auto entity : view) {
        auto& panel = view.get<GUIPanel>(entity);
        if (panel.panelId == panelId && !panel.isOpen) {
            panel.isOpen = true;
            // Reset cursor to center when opening panel
            if (cursorEntity != entt::null && registry.valid(cursorEntity)) {
                auto& cursor = registry.get<GUICursor>(cursorEntity);
                cursor.position = screenSize * 0.5f;
            }
            eventBus.emit(events::PanelOpenedEvent{panelId, entity});
            break;
        }
    }
}

void GUISystem::ClosePanel(const std::string& panelId) {
    auto view = registry.view<GUIPanel>();
    for (auto entity : view) {
        auto& panel = view.get<GUIPanel>(entity);
        if (panel.panelId == panelId && panel.isOpen) {
            panel.isOpen = false;
            // Clear hovered element
            if (cursorEntity != entt::null && registry.valid(cursorEntity)) {
                auto& cursor = registry.get<GUICursor>(cursorEntity);
                cursor.hoveredElement = entt::null;
            }
            eventBus.emit(events::PanelClosedEvent{panelId, entity});
            break;
        }
    }
}

void GUISystem::TogglePanel(const std::string& panelId) {
    auto view = registry.view<GUIPanel>();
    for (auto entity : view) {
        auto& panel = view.get<GUIPanel>(entity);
        if (panel.panelId == panelId) {
            panel.isOpen = !panel.isOpen;
            if (panel.isOpen) {
                if (cursorEntity != entt::null && registry.valid(cursorEntity)) {
                    auto& cursor = registry.get<GUICursor>(cursorEntity);
                    cursor.position = screenSize * 0.5f;
                }
                eventBus.emit(events::PanelOpenedEvent{panelId, entity});
            } else {
                if (cursorEntity != entt::null && registry.valid(cursorEntity)) {
                    auto& cursor = registry.get<GUICursor>(cursorEntity);
                    cursor.hoveredElement = entt::null;
                }
                eventBus.emit(events::PanelClosedEvent{panelId, entity});
            }
            break;
        }
    }
}

bool GUISystem::IsBlockingInput() const {
    auto view = registry.view<GUIPanel>();
    for (auto entity : view) {
        const auto& panel = view.get<GUIPanel>(entity);
        if (panel.isOpen && panel.blocksGameInput) {
            return true;
        }
    }
    return false;
}

} // namespace ecs::systems
