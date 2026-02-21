#pragma once

#include <string>

namespace ASCIICraft {

/// Abstract source of game input (action-based).
/// Used so systems can read input without depending on InputManager or InputSystem.
/// Implementations report engine state; Game orchestrates who runs when (e.g. skips movement/camera when GUI blocks).
struct IInputSource {
    virtual ~IInputSource() = default;

    virtual bool IsActionHeld(const std::string& action) const = 0;
    virtual bool IsActionPressed(const std::string& action) const = 0;
    virtual float GetMouseSensitivity() const = 0;
};

} // namespace ASCIICraft
