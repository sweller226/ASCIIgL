#pragma once

#include <ASCIICraft/input/IInputSource.hpp>

class EventBus;

namespace input {

/// Who is consuming input this frame: gameplay (movement, look, jump, etc.) or GUI.
enum class InputMode {
    Gameplay,
    GUI,
};

/// Central input. Calls InputManager::Update() once per frame, emits discrete input events,
/// and exposes action state (pass-through from engine).
///
/// When mode is GUI, Update() does not emit gameplay events. Other input (e.g. inventory toggle) is unaffected.
class InputSystem : public IInputSource {
public:
    explicit InputSystem(EventBus& eventBus);

    void Update();

    void SetInputMode(InputMode mode);
    InputMode GetInputMode() const { return m_inputMode; }

    // IInputSource — pass-through from engine
    bool IsActionHeld(const std::string& action) const override;
    bool IsActionPressed(const std::string& action) const override;
    float GetMouseSensitivity() const override;

private:
    EventBus& m_eventBus;
    InputMode m_inputMode = InputMode::Gameplay;
};

} // namespace input
