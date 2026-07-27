#pragma once

#include <ASCIICraft/input/IInputSource.hpp>
#include <ASCIICraft/input/InputSystem.hpp>
#include <glm/vec2.hpp>

/// Wraps the real input source and blocks all gameplay input when GUI is active.
/// Pass this to MovementSystem and CameraSystem so they see "no input" when the GUI is open,
/// while GUIManager continues to use the real InputSystem for cursor and clicks.
class GameplayInputFilter : public IInputSource {
public:
    explicit GameplayInputFilter(input::InputSystem& inputSystem)
        : m_inputSystem(inputSystem) {}

    bool IsActionHeld(const std::string& action) const override {
        if (m_inputSystem.GetInputMode() != input::InputMode::Gameplay)
            return false;
        return m_inputSystem.IsActionHeld(action);
    }

    bool IsActionPressed(const std::string& action) const override {
        if (m_inputSystem.GetInputMode() != input::InputMode::Gameplay)
            return false;
        return m_inputSystem.IsActionPressed(action);
    }

    float GetMouseSensitivity() const override {
        return m_inputSystem.GetMouseSensitivity();
    }

    float GetKeyboardLookSpeed() const override {
        return m_inputSystem.GetKeyboardLookSpeed();
    }

    bool IsMouseLookEnabled() const override {
        return m_inputSystem.IsMouseLookEnabled();
    }

    glm::vec2 GetLookDelta() const override {
        if (m_inputSystem.GetInputMode() != input::InputMode::Gameplay)
            return {0.0f, 0.0f};
        return m_inputSystem.GetLookDelta();
    }

    glm::vec2 GetPointerPosition() const override {
        return m_inputSystem.GetPointerPosition();
    }

    int GetScrollDelta() const override {
        if (m_inputSystem.GetInputMode() != input::InputMode::Gameplay)
            return 0;
        return m_inputSystem.GetScrollDelta();
    }

private:
    input::InputSystem& m_inputSystem;
};
