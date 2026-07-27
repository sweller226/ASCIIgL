#pragma once

#include <ASCIICraft/input/IInputSource.hpp>
#include <glm/vec2.hpp>

namespace ASCIIgL { class EventBus; }

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
    explicit InputSystem(ASCIIgL::EventBus& eventBus);

    void Update();

    void SetInputMode(InputMode mode);
    InputMode GetInputMode() const { return m_inputMode; }

    void SetMouseLookEnabled(bool enabled);
    bool IsMouseLookEnabled() const override;

    // IInputSource — pass-through from engine
    bool IsActionHeld(const std::string& action) const override;
    bool IsActionPressed(const std::string& action) const override;
    float GetMouseSensitivity() const override;
    glm::vec2 GetLookDelta() const override;
    glm::vec2 GetPointerPosition() const override;
    int GetScrollDelta() const override;

private:
    void SyncMouseCaptureState();

    ASCIIgL::EventBus& m_eventBus;
    InputMode m_inputMode = InputMode::Gameplay;
    bool m_useMouseLook = false;
    glm::vec2 m_lookDelta{0.0f, 0.0f};
    glm::vec2 m_pointerScreenPos{0.0f, 0.0f};
    int m_scrollDelta = 0;
};

} // namespace input
