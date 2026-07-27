#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/util/EventBus.hpp>
#include <ASCIICraft/events/InputEvents.hpp>
#include <ASCIICraft/events/ItemEvents.hpp>
#include <ASCIICraft/events/GUIEvents.hpp>
#include <ASCIICraft/input/InputSystem.hpp>

namespace input {

namespace {

bool ActionMatchesMouseButton(const std::string& action, ASCIIgL::InputManager::MouseButton& outButton) {
    if (action == "interact_left") {
        outButton = ASCIIgL::InputManager::MouseButton::Left;
        return true;
    }
    if (action == "interact_right") {
        outButton = ASCIIgL::InputManager::MouseButton::Right;
        return true;
    }
    return false;
}

} // namespace

InputSystem::InputSystem(ASCIIgL::EventBus& eventBus)
    : m_eventBus(eventBus)
{}

void InputSystem::SetInputMode(InputMode newMode) {
    if (m_inputMode == newMode) {
        return;
    }
    m_inputMode = newMode;
    SyncMouseCaptureState();
    // After releasing capture for GUI, refresh pointer so hotspot sync isn't one frame stale.
    if (m_inputMode == InputMode::GUI) {
        m_pointerScreenPos = ASCIIgL::InputManager::GetInst().GetPointerScreenPos();
    }
}

void InputSystem::SetMouseLookEnabled(bool enabled) {
    if (m_useMouseLook == enabled) {
        return;
    }
    m_useMouseLook = enabled;
    ASCIIgL::InputManager::GetInst().SetTerminalMouseTracking(m_useMouseLook);
    SyncMouseCaptureState();
}

bool InputSystem::IsMouseLookEnabled() const {
    return m_useMouseLook;
}

void InputSystem::SyncMouseCaptureState() {
    auto& input = ASCIIgL::InputManager::GetInst();
    const bool capture = m_useMouseLook
        && m_inputMode == InputMode::Gameplay
        && input.IsHostFocused();
    input.SetMouseCapture(capture);
}

void InputSystem::Update() {
    auto& input = ASCIIgL::InputManager::GetInst();

    // Keep capture gated on focus each frame (alt-tab releases look).
    // Must run before InputManager::Update so relative deltas are sampled this frame.
    SyncMouseCaptureState();

    input.Update();

    if (m_useMouseLook && m_inputMode == InputMode::Gameplay) {
        m_lookDelta = input.GetMouseDelta();
    } else {
        m_lookDelta = {0.0f, 0.0f};
        // Consume any residual capture delta so it does not leak later.
        (void)input.GetMouseDelta();
    }

    m_pointerScreenPos = input.GetPointerScreenPos();
    const int rawScroll = input.ConsumeScrollDelta();
    m_scrollDelta = (m_inputMode == InputMode::Gameplay) ? rawScroll : 0;

    // Inventory toggle (E) — always emit so GUI can close with E when inventory is open.
    if (input.IsActionPressed("interact")) {
        m_eventBus.emit(events::ToggleInventoryEvent{});
    }

    // Drop item (Q) — always emit; works in GUI (carried / hovered slot) and gameplay (hotbar).
    if (input.IsActionPressed("drop_item")) {
        m_eventBus.emit(events::DropItemPressedEvent{});
    }

    // Gameplay events only when in Gameplay mode (e.g. no GUI blocking).
    // Use InputSystem action queries so mouse buttons map to F/R when mouse look is on.
    if (m_inputMode == InputMode::Gameplay) {
        if (IsActionPressed("quit"))    m_eventBus.emit(events::QuitRequestedEvent{});
        if (IsActionPressed("interact_left"))  m_eventBus.emit(events::PrimaryActionPressedEvent{});
        if (IsActionHeld("interact_left"))     m_eventBus.emit(events::PrimaryActionHeldEvent{});
        if (IsActionPressed("interact_right")) m_eventBus.emit(events::SecondaryActionPressedEvent{});
        // Game mode toggle (P) — hard-wired for now, not action-bound.
        if (input.IsKeyPressed(ASCIIgL::Key::P)) m_eventBus.emit(events::SwitchGameModeEvent{});
    }
}

bool InputSystem::IsActionHeld(const std::string& action) const {
    auto& input = ASCIIgL::InputManager::GetInst();
    ASCIIgL::InputManager::MouseButton button;
    if (m_useMouseLook && ActionMatchesMouseButton(action, button)) {
        // Mouse mode: F/R are unbound; only LMB/RMB drive interact_left/right.
        return input.IsMouseButtonDown(button);
    }
    return input.IsActionHeld(action);
}

bool InputSystem::IsActionPressed(const std::string& action) const {
    auto& input = ASCIIgL::InputManager::GetInst();
    ASCIIgL::InputManager::MouseButton button;
    if (m_useMouseLook && ActionMatchesMouseButton(action, button)) {
        // Mouse mode: F/R are unbound; only LMB/RMB drive interact_left/right.
        return input.IsMouseButtonPressed(button);
    }
    return input.IsActionPressed(action);
}

float InputSystem::GetMouseSensitivity() const {
    return ASCIIgL::InputManager::GetInst().GetMouseSensitivity();
}

glm::vec2 InputSystem::GetLookDelta() const {
    return m_lookDelta;
}

glm::vec2 InputSystem::GetPointerPosition() const {
    return m_pointerScreenPos;
}

int InputSystem::GetScrollDelta() const {
    return m_scrollDelta;
}

} // namespace input
