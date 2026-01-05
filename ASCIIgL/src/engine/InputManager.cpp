#include <ASCIIgL/engine/InputManager.hpp>
#include <algorithm>

// Platform-specific implementations
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    
namespace ASCIIgL {

    // Windows-specific implementation using GetAsyncKeyState
    class InputManager::InputManagerImpl {
    public:
        InputManagerImpl() = default;
        ~InputManagerImpl() = default;

        bool IsKeyDown(Key key) const {
            return IsVirtualKeyDown(static_cast<int>(key));
        }

        void UpdateKeyboard() {
            // Windows handles keyboard state through polling in IsKeyDown
            // No additional per-frame update needed
        }

    private:
        bool IsVirtualKeyDown(int virtualKey) const {
            // GetAsyncKeyState returns the current hardware state
            // Bit 0x8000 is set if the key is currently pressed
            return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
        }
    };

} // namespace ASCIIgL

#else
    // Placeholder for other platforms (Linux, macOS)
    // These would use different APIs (X11, Cocoa, etc.)
namespace ASCIIgL {

    class InputManager::InputManagerImpl {
    public:
        InputManagerImpl() = default;
        ~InputManagerImpl() = default;

        bool IsKeyDown(Key key) const {
            // TODO: Implement for other platforms
            return false;
        }

        void UpdateKeyboard() {
            // TODO: Implement for other platforms
        }
    };

} // namespace ASCIIgL

#endif

namespace ASCIIgL {

// ============================================================================
// InputManager Implementation
// ============================================================================

InputManager::InputManager() 
    : pImpl(std::make_unique<InputManagerImpl>()) {
}

InputManager::~InputManager() = default;

void InputManager::Initialize() {
    // Set up common default bindings that most games will use
    // Games can override these or add more after calling Initialize()
    
    // Movement (WASD)
    BindKey(Key::W, "move_forward");
    BindKey(Key::A, "move_left");
    BindKey(Key::S, "move_backward");
    BindKey(Key::D, "move_right");
    
    // Actions
    BindKey(Key::SPACE, "jump");
    BindKey(Key::SHIFT, "sneak");
    BindKey(Key::CTRL, "sprint");
    
    // Camera (arrow keys)
    BindKey(Key::LEFT, "camera_left");
    BindKey(Key::RIGHT, "camera_right");
    BindKey(Key::UP, "camera_up");
    BindKey(Key::DOWN, "camera_down");
    
    // Common game actions
    BindKey(Key::E, "interact");
    BindKey(Key::Q, "interact_left");
    BindKey(Key::R, "interact_right");
    BindKey(Key::ESCAPE, "quit");
    BindKey(Key::ENTER, "confirm");
    BindKey(Key::TAB, "menu");
    
    // Hotbar/inventory slots
    BindKey(Key::NUM_1, "hotbar_1");
    BindKey(Key::NUM_2, "hotbar_2");
    BindKey(Key::NUM_3, "hotbar_3");
    BindKey(Key::NUM_4, "hotbar_4");
    BindKey(Key::NUM_5, "hotbar_5");
    BindKey(Key::NUM_6, "hotbar_6");
    BindKey(Key::NUM_7, "hotbar_7");
    BindKey(Key::NUM_8, "hotbar_8");
    BindKey(Key::NUM_9, "hotbar_9");
    BindKey(Key::NUM_0, "hotbar_0");
}

void InputManager::Update() {
    // Store previous frame's key states
    previousKeyStates = keyStates;
    
    // Update platform-specific input
    pImpl->UpdateKeyboard();
    
    // Update all tracked keys
    for (auto& [key, state] : keyStates) {
        state = CalculateKeyState(key);
    }
    
    // Scan for new key presses (keys not yet in our tracking map)
    // Only scan common key ranges to avoid excessive polling
    // 0x08-0x2E: Special keys (backspace through delete)
    // 0x30-0x5A: Numbers and letters
    // 0x60-0x7B: Numpad and function keys
    // 0xA0-0xA5: Left/Right modifier keys
    static const std::pair<int, int> keyRanges[] = {
        {0x08, 0x2E},
        {0x30, 0x5A},
        {0x60, 0x7B},
        {0xA0, 0xA5},
        {0xBA, 0xDE}  // Punctuation
    };
    
    for (const auto& [start, end] : keyRanges) {
        for (int keyCode = start; keyCode <= end; ++keyCode) {
            Key key = static_cast<Key>(keyCode);
            if (keyStates.find(key) == keyStates.end() && pImpl->IsKeyDown(key)) {
                keyStates[key] = CalculateKeyState(key);
                previousKeyStates[key] = InputState::Released;
            }
        }
    }
    
    // Update toggle action states
    UpdateToggleStates();
}

// ============================================================================
// Key State Queries
// ============================================================================

bool InputManager::IsKeyPressed(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::Pressed;
}

bool InputManager::IsKeyHeld(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::Held;
}

bool InputManager::IsKeyReleased(Key key) const {
    // A key is "released" if it was down last frame but not this frame
    auto prevIt = previousKeyStates.find(key);
    auto currIt = keyStates.find(key);
    
    bool wasPreviouslyDown = prevIt != previousKeyStates.end() && 
                              (prevIt->second == InputState::Pressed || prevIt->second == InputState::Held);
    bool isCurrentlyDown = currIt != keyStates.end() && 
                           (currIt->second == InputState::Pressed || currIt->second == InputState::Held);
    
    return wasPreviouslyDown && !isCurrentlyDown;
}

bool InputManager::IsKeyDown(Key key) const {
    auto it = keyStates.find(key);
    if (it != keyStates.end()) {
        return it->second == InputState::Pressed || it->second == InputState::Held;
    }
    return false;
}

InputState InputManager::GetKeyState(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() ? it->second : InputState::Released;
}

// ============================================================================
// Action Binding
// ============================================================================

void InputManager::BindKey(Key key, const std::string& action, bool isToggle) {
    // Remove any existing binding for this key
    UnbindKey(key);
    
    // Remove any existing binding for this action
    UnbindAction(action);
    
    // Create new binding
    keyBindings[key] = KeyBinding(key, action, isToggle);
    actionToKey[action] = key;
    
    // Initialize toggle state if needed
    if (isToggle) {
        toggleStates[action] = false;
    }
}

void InputManager::UnbindKey(Key key) {
    auto it = keyBindings.find(key);
    if (it != keyBindings.end()) {
        // Remove from action mapping
        actionToKey.erase(it->second.action);
        
        // Remove toggle state if applicable
        if (it->second.isToggle) {
            toggleStates.erase(it->second.action);
        }
        
        // Remove binding
        keyBindings.erase(it);
    }
}

void InputManager::UnbindAction(const std::string& action) {
    auto it = actionToKey.find(action);
    if (it != actionToKey.end()) {
        UnbindKey(it->second);
    }
}

void InputManager::ClearBindings() {
    keyBindings.clear();
    actionToKey.clear();
    toggleStates.clear();
}

// ============================================================================
// Action State Queries
// ============================================================================

bool InputManager::IsActionPressed(const std::string& action) const {
    auto it = actionToKey.find(action);
    if (it != actionToKey.end()) {
        return IsKeyPressed(it->second);
    }
    return false;
}

bool InputManager::IsActionHeld(const std::string& action) const {
    auto it = actionToKey.find(action);
    if (it != actionToKey.end()) {
        auto bindingIt = keyBindings.find(it->second);
        if (bindingIt != keyBindings.end()) {
            if (bindingIt->second.isToggle) {
                // For toggle actions, return the current toggle state
                auto stateIt = toggleStates.find(action);
                return stateIt != toggleStates.end() && stateIt->second;
            } else {
                // For regular actions, check if key is down
                return IsKeyDown(it->second);
            }
        }
    }
    return false;
}

bool InputManager::IsActionReleased(const std::string& action) const {
    auto it = actionToKey.find(action);
    if (it != actionToKey.end()) {
        return IsKeyReleased(it->second);
    }
    return false;
}

bool InputManager::GetToggleState(const std::string& action) const {
    auto it = toggleStates.find(action);
    return it != toggleStates.end() ? it->second : false;
}

void InputManager::SetToggleState(const std::string& action, bool state) {
    auto it = toggleStates.find(action);
    if (it != toggleStates.end()) {
        it->second = state;
    }
}

// ============================================================================
// Utility
// ============================================================================

void InputManager::ClearInputState() {
    keyStates.clear();
    previousKeyStates.clear();
    // Reset all toggle states to false
    for (auto& [action, state] : toggleStates) {
        state = false;
    }
}

Key InputManager::GetKeyForAction(const std::string& action) const {
    auto it = actionToKey.find(action);
    return it != actionToKey.end() ? it->second : Key::NONE;
}

std::string InputManager::GetActionForKey(Key key) const {
    auto it = keyBindings.find(key);
    return it != keyBindings.end() ? it->second.action : "";
}

void InputManager::SetMouseSensitivity(float sensitivity) {
    mouseSensitivity = std::max(0.1f, sensitivity);
}

float InputManager::GetMouseSensitivity() const {
    return mouseSensitivity;
}

// ============================================================================
// Private Helpers
// ============================================================================

InputState InputManager::CalculateKeyState(Key key) const {
    // Get current hardware state
    bool isCurrentlyDown = pImpl->IsKeyDown(key);
    
    // Get previous frame state
    auto prevIt = previousKeyStates.find(key);
    bool wasPreviouslyDown = (prevIt != previousKeyStates.end() && 
                              (prevIt->second == InputState::Pressed || 
                               prevIt->second == InputState::Held));
    
    // Calculate state based on transitions
    if (isCurrentlyDown && !wasPreviouslyDown) {
        return InputState::Pressed;  // Just pressed this frame
    }
    else if (isCurrentlyDown && wasPreviouslyDown) {
        return InputState::Held;     // Held down from previous frame
    }
    else {
        return InputState::Released; // Not pressed
    }
}

void InputManager::UpdateToggleStates() {
    for (auto& [action, toggleState] : toggleStates) {
        auto keyIt = actionToKey.find(action);
        if (keyIt != actionToKey.end()) {
            if (IsKeyPressed(keyIt->second)) {
                toggleState = !toggleState;  // Flip toggle state on key press
            }
        }
    }
}

} // namespace ASCIIgL
