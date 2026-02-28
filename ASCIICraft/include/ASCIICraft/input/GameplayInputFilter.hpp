#pragma once

#include <ASCIICraft/input/IInputSource.hpp>
#include <ASCIICraft/input/InputSystem.hpp>

/// Wraps the real input source and blocks all gameplay input when GUI is active.
/// Pass this to MovementSystem and CameraSystem so they see "no input" when the GUI is open,
/// while GuiManager continues to use the real InputSystem for cursor and clicks.
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

private:
    input::InputSystem& m_inputSystem;
};
