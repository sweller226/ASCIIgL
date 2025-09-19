#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <memory>

// Key codes - using standard virtual key codes for Windows
enum class Key {
    // Movement keys
    W = 0x57,
    A = 0x41,
    S = 0x53,
    D = 0x44,
    
    // Action keys
    SPACE = 0x20,           // Jump
    SHIFT = 0x10,           // Sneak/Sprint
    CTRL = 0x11,            // Sprint (alternative)

    // temporary keys to replace camera controls
    Q = 0x51,               // mouse left
    R = 0x52,               // Cmouse right

    // Arrow keys (Controls camera and gui elements)
    UP = 0x26, // GUI up or camera up
    DOWN = 0x28, // GUI down or camera down
    LEFT = 0x25, // GUI left or camera left
    RIGHT = 0x27, // GUI right or camera right

    // Number keys (hotbar)
    NUM_1 = 0x31,
    NUM_2 = 0x32,
    NUM_3 = 0x33,
    NUM_4 = 0x34,
    NUM_5 = 0x35,
    NUM_6 = 0x36,
    NUM_7 = 0x37,
    NUM_8 = 0x38,
    NUM_9 = 0x39,
    
    // Other keys
    ESCAPE = 0x1B,          // Menu
    ENTER = 0x0D,           // Enter
    E = 0x45,               // Inventory
    
    // Special
    NONE = 0x0000
};

enum class InputState {
    Released,
    Pressed,
    Held
};

struct KeyBinding {
    Key key;
    std::string action;
    bool isToggle;
    
    // Default constructor for std::unordered_map
    KeyBinding() : key(Key::NONE), action(""), isToggle(false) {}
    
    KeyBinding(Key k, const std::string& a, bool toggle = false) 
        : key(k), action(a), isToggle(toggle) {}
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    // Non-copyable, non-movable (due to unique_ptr)
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    // Core update functions
    void Update();
    
    // Key state queries
    bool IsKeyPressed(Key key) const;
    bool IsKeyHeld(Key key) const;
    bool IsKeyReleased(Key key) const;
    InputState GetKeyState(Key key) const;
    
    // Key and action binding
    void BindKey(Key key, const std::string& action, bool isToggle = false);
    void UnbindKey(Key key);
    void UnbindAction(const std::string& action);
    
    // Action state queries
    bool IsActionPressed(const std::string& action) const;
    bool IsActionHeld(const std::string& action) const;
    bool IsActionReleased(const std::string& action) const;
    
    // Settings
    void SetMouseSensitivity(float sensitivity);
    float GetMouseSensitivity() const;
    
    // Utility functions
    void ClearInputState();
    
    // Default key bindings
    void SetDefaultBindings();

private:
    // PIMPL pattern - hide platform-specific implementation
    class InputManagerImpl;
    std::unique_ptr<InputManagerImpl> pImpl;
    
    // Platform-agnostic data
    std::unordered_map<Key, InputState> keyStates;
    std::unordered_map<Key, InputState> previousKeyStates;
    
    std::unordered_map<Key, KeyBinding> keyBindings;
    std::unordered_map<std::string, Key> actionToKey;
    std::unordered_map<std::string, bool> actionStates;
    
    float mouseSensitivity;
    
    // Platform-agnostic helper functions
    InputState CalculateKeyState(Key key) const;
    void UpdateActionStates();
    
    static constexpr float DEFAULT_MOUSE_SENSITIVITY = 60.0f;
};