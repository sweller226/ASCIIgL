#include <ASCIICraft/input/InputSystem.hpp>

#include <ASCIICraft/events/EventBus.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIIgL/engine/InputManager.hpp>

namespace ASCIICraft::input {

InputSystem::InputSystem(EventBus& eventBus)
    : m_eventBus(eventBus)
{}

void InputSystem::SetInputMode(InputMode newMode) {
    m_inputMode = newMode;
}

void InputSystem::Update() {
    ASCIIgL::InputManager::GetInst().Update();

    auto& input = ASCIIgL::InputManager::GetInst();

    // Inventory toggle (E) — always emit so GUI can close with E when inventory is open.
    if (input.IsActionPressed("interact")) {
        m_eventBus.emit(events::ToggleInventoryEvent{});
    }

    // Gameplay events only when in Gameplay mode (e.g. no GUI blocking).
    if (m_inputMode == InputMode::Gameplay) {
        if (input.IsActionPressed("quit"))    m_eventBus.emit(events::QuitRequestedEvent{});
        if (input.IsActionPressed("jump"))    m_eventBus.emit(events::JumpPressedEvent{});
        if (input.IsActionPressed("interact_left"))  m_eventBus.emit(events::PrimaryActionPressedEvent{});
        if (input.IsActionPressed("interact_right")) m_eventBus.emit(events::SecondaryActionPressedEvent{});
        if (input.IsActionPressed("attack"))          m_eventBus.emit(events::AttackActionPressedEvent{});
    }
}

bool InputSystem::IsActionHeld(const std::string& action) const {
    return ASCIIgL::InputManager::GetInst().IsActionHeld(action);
}

bool InputSystem::IsActionPressed(const std::string& action) const {
    return ASCIIgL::InputManager::GetInst().IsActionPressed(action);
}

float InputSystem::GetMouseSensitivity() const {
    return ASCIIgL::InputManager::GetInst().GetMouseSensitivity();
}

} // namespace ASCIICraft::input
