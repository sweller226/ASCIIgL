#include <ASCIICraft/input_manager/InputManager.hpp>
#include <algorithm>

// Platform-specific implementations
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <Windows.h>
    
    // Windows-specific implementation
    class InputManager::InputManagerImpl {
    public:
        InputManagerImpl() = default;
        ~InputManagerImpl() = default;

        bool IsKeyDown(Key key) const {
            return IsVirtualKeyDown(static_cast<int>(key));
        }

        void UpdateKeyboard() {
            // Platform-specific keyboard update - Windows handles this through polling
        }

    private:
        bool IsVirtualKeyDown(int virtualKey) const {
            // Use GetAsyncKeyState for real-time key checking
            return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
        }
    };

#else
    // going to add general implementation for other platforms later
#endif

// InputManager implementation
InputManager::InputManager() 
    : pImpl(std::make_unique<InputManagerImpl>())
    , mouseSensitivity(DEFAULT_MOUSE_SENSITIVITY) {
    SetDefaultBindings();
}

InputManager::~InputManager() = default;

void InputManager::Update() {
    // Store previous frame's key states
    previousKeyStates = keyStates;
    
    // Update platform-specific input
    pImpl->UpdateKeyboard();
    
    // Update all tracked keys
    for (auto& [key, state] : keyStates) {
        state = CalculateKeyState(key);
    }
    
    // Also check for new keys that might have been pressed
    for (int keyCode = 0x08; keyCode <= 0xFF; ++keyCode) {
        Key key = static_cast<Key>(keyCode);
        if (keyStates.find(key) == keyStates.end() && pImpl->IsKeyDown(key)) {
            keyStates[key] = CalculateKeyState(key);
            previousKeyStates[key] = InputState::Released;
        }
    }
    
    UpdateActionStates();
}

bool InputManager::IsKeyPressed(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::Pressed;
}

bool InputManager::IsKeyHeld(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::Held;
}

bool InputManager::IsKeyReleased(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::Released;
}

InputState InputManager::GetKeyState(Key key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() ? it->second : InputState::Released;
}

void InputManager::BindKey(Key key, const std::string& action, bool isToggle) {
    // Remove existing binding for this key if it exists
    UnbindKey(key);
    
    // Create new binding
    keyBindings[key] = KeyBinding(key, action, isToggle);
    actionToKey[action] = key;
    
    // Initialize action state for toggle actions
    if (isToggle) {
        actionStates[action] = false;
    }
}

void InputManager::UnbindKey(Key key) {
    auto it = keyBindings.find(key);
    if (it != keyBindings.end()) {
        // Remove from action mapping
        actionToKey.erase(it->second.action);
        
        // Remove action state if it was a toggle
        if (it->second.isToggle) {
            actionStates.erase(it->second.action);
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

bool InputManager::IsActionPressed(const std::string& action) const {
    auto it = actionToKey.find(action);
    if (it != actionToKey.end()) {
        auto bindingIt = keyBindings.find(it->second);
        if (bindingIt != keyBindings.end()) {
            if (bindingIt->second.isToggle) {
                // For toggle actions, return true when the toggle state changes to true
                auto stateIt = actionStates.find(action);
                return stateIt != actionStates.end() && stateIt->second && IsKeyPressed(it->second);
            } else {
                return IsKeyPressed(it->second);
            }
        }
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
                auto stateIt = actionStates.find(action);
                return stateIt != actionStates.end() && stateIt->second;
            } else {
                return IsKeyHeld(it->second) || IsKeyPressed(it->second);
            }
        }
    }
    return false;
}

bool InputManager::IsActionReleased(const std::string& action) const {
    auto it = actionToKey.find(action);
    if (it != actionToKey.end()) {
        auto bindingIt = keyBindings.find(it->second);
        if (bindingIt != keyBindings.end()) {
            if (bindingIt->second.isToggle) {
                // For toggle actions, return true when the toggle state changes to false
                auto stateIt = actionStates.find(action);
                return stateIt != actionStates.end() && !stateIt->second && IsKeyPressed(it->second);
            } else {
                return IsKeyReleased(it->second);
            }
        }
    }
    return false;
}

void InputManager::SetMouseSensitivity(float sensitivity) {
    mouseSensitivity = std::max(0.1f, sensitivity);
}

float InputManager::GetMouseSensitivity() const {
    return mouseSensitivity;
}

void InputManager::ClearInputState() {
    keyStates.clear();
    previousKeyStates.clear();
    actionStates.clear();
}

void InputManager::SetDefaultBindings() {
    // Movement
    BindKey(Key::W, "move_forward");
    BindKey(Key::A, "move_left");
    BindKey(Key::S, "move_backward");
    BindKey(Key::D, "move_right");
    
    // Actions
    BindKey(Key::SPACE, "jump");
    BindKey(Key::SHIFT, "sneak");
    BindKey(Key::CTRL, "sprint");
    
    // Block interaction (mouse replacement)
    BindKey(Key::Q, "interact_left");        // Left click replacement
    BindKey(Key::R, "interact_right");        // Right click replacement
    
    // Camera (arrow keys only)
    BindKey(Key::LEFT, "camera_left");
    BindKey(Key::RIGHT, "camera_right");
    BindKey(Key::UP, "camera_up");
    BindKey(Key::DOWN, "camera_down");
    
    // Game actions
    BindKey(Key::E, "open_inventory");
    BindKey(Key::ESCAPE, "quit");
    BindKey(Key::ENTER, "confirm");
    
    // Hotbar slots
    BindKey(Key::NUM_1, "hotbar_1");
    BindKey(Key::NUM_2, "hotbar_2");
    BindKey(Key::NUM_3, "hotbar_3");
    BindKey(Key::NUM_4, "hotbar_4");
    BindKey(Key::NUM_5, "hotbar_5");
    BindKey(Key::NUM_6, "hotbar_6");
    BindKey(Key::NUM_7, "hotbar_7");
    BindKey(Key::NUM_8, "hotbar_8");
    BindKey(Key::NUM_9, "hotbar_9");
}

void InputManager::UpdateActionStates() {
    for (auto& [action, toggleState] : actionStates) {
        auto keyIt = actionToKey.find(action);
        if (keyIt != actionToKey.end()) {
            if (IsKeyPressed(keyIt->second)) {
                toggleState = !toggleState;  // Flip toggle state on key press
            }
        }
    }
}

InputState InputManager::CalculateKeyState(Key key) const {
    // Get current hardware state
    bool isCurrentlyDown = pImpl->IsKeyDown(key);
    
    // Get previous frame state
    auto prevIt = previousKeyStates.find(key);
    bool wasPreviouslyDown = (prevIt != previousKeyStates.end() && 
                             (prevIt->second == InputState::Pressed || prevIt->second == InputState::Held));
    
    // Calculate state based on transitions
    if (isCurrentlyDown && !wasPreviouslyDown) {
        return InputState::Pressed;  // Just pressed this frame
    }
    else if (isCurrentlyDown && wasPreviouslyDown) {
        return InputState::Held;     // Held down from previous frame
    }
    else {
        return InputState::Released; // Not pressed or just released
    }
}
