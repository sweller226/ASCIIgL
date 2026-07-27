#pragma once

#include <string>
#include <glm/vec2.hpp>

/// Abstract source of game input (action-based).
/// Used so systems can read input without depending on InputManager or InputSystem.
/// Implementations report engine state; Game orchestrates who runs when (e.g. skips movement/camera when GUI blocks).
struct IInputSource {
    virtual ~IInputSource() = default;

    virtual bool IsActionHeld(const std::string& action) const = 0;
    virtual bool IsActionPressed(const std::string& action) const = 0;
    virtual float GetMouseSensitivity() const = 0;

    /// True when the mouse look control scheme is active (vs keyboard arrows).
    virtual bool IsMouseLookEnabled() const = 0;
    /// 3D look: pixel delta this frame while captured; {0,0} otherwise.
    virtual glm::vec2 GetLookDelta() const = 0;
    /// 2D GUI: pointer in game screen space (Screen cells); valid when mouse look is enabled.
    virtual glm::vec2 GetPointerPosition() const = 0;
    /// Mouse wheel notches this frame (positive = up / away). 0 when GUI mode.
    virtual int GetScrollDelta() const = 0;
};
