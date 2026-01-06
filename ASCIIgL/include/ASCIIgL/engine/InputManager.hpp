#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <functional>

namespace ASCIIgL {

/**
 * Virtual key codes for keyboard input
 * Uses standard Windows virtual key codes for cross-platform compatibility
 */
enum class Key {
    // Letters
    A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46, G = 0x47,
    H = 0x48, I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C, M = 0x4D, N = 0x4E,
    O = 0x4F, P = 0x50, Q = 0x51, R = 0x52, S = 0x53, T = 0x54, U = 0x55,
    V = 0x56, W = 0x57, X = 0x58, Y = 0x59, Z = 0x5A,
    
    // Numbers (top row)
    NUM_0 = 0x30, NUM_1 = 0x31, NUM_2 = 0x32, NUM_3 = 0x33, NUM_4 = 0x34,
    NUM_5 = 0x35, NUM_6 = 0x36, NUM_7 = 0x37, NUM_8 = 0x38, NUM_9 = 0x39,
    
    // Function keys
    F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75,
    F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,
    
    // Arrow keys
    LEFT = 0x25, UP = 0x26, RIGHT = 0x27, DOWN = 0x28,
    
    // Modifier keys
    SHIFT = 0x10, CTRL = 0x11, ALT = 0x12,
    LSHIFT = 0xA0, RSHIFT = 0xA1,
    LCTRL = 0xA2, RCTRL = 0xA3,
    LALT = 0xA4, RALT = 0xA5,
    
    // Special keys
    SPACE = 0x20,
    ENTER = 0x0D,
    TAB = 0x09,
    ESCAPE = 0x1B,
    BACKSPACE = 0x08,
    DELETE_KEY = 0x2E,  // Named DELETE_KEY to avoid macro conflicts
    INSERT = 0x2D,
    HOME = 0x24,
    END = 0x23,
    PAGE_UP = 0x21,
    PAGE_DOWN = 0x22,
    
    // Punctuation/symbols
    SEMICOLON = 0xBA,     // ;
    EQUALS = 0xBB,        // =
    COMMA = 0xBC,         // ,
    MINUS = 0xBD,         // -
    PERIOD = 0xBE,        // .
    SLASH = 0xBF,         // /
    BACKTICK = 0xC0,      // `
    LBRACKET = 0xDB,      // [
    BACKSLASH = 0xDC,     // backslash
    RBRACKET = 0xDD,      // ]
    QUOTE = 0xDE,         // '
    
    // Numpad
    NUMPAD_0 = 0x60, NUMPAD_1 = 0x61, NUMPAD_2 = 0x62, NUMPAD_3 = 0x63,
    NUMPAD_4 = 0x64, NUMPAD_5 = 0x65, NUMPAD_6 = 0x66, NUMPAD_7 = 0x67,
    NUMPAD_8 = 0x68, NUMPAD_9 = 0x69,
    NUMPAD_MULTIPLY = 0x6A,
    NUMPAD_ADD = 0x6B,
    NUMPAD_SUBTRACT = 0x6D,
    NUMPAD_DECIMAL = 0x6E,
    NUMPAD_DIVIDE = 0x6F,
    
    // Null/None key
    NONE = 0x0000
};

/**
 * @brief Represents the state of a key for a given frame
 */
enum class InputState {
    Released,   // Key is not pressed
    Pressed,    // Key was just pressed this frame
    Held        // Key is being held down (pressed on a previous frame)
};

/**
 * Represents a binding between a key and a named action
 */
struct KeyBinding {
    Key key;
    std::string action;
    bool isToggle;  // If true, the action state flips each time the key is pressed
    
    KeyBinding() : key(Key::NONE), action(""), isToggle(false) {}
    KeyBinding(Key k, const std::string& a, bool toggle = false) 
        : key(k), action(a), isToggle(toggle) {}
};

/**
 * Singleton input manager for handling keyboard input
 * 
 * Provides frame-based input state tracking with support for:
 * - Raw key state queries (pressed, held, released)
 * - Action-based input with named bindings
 * - Toggle actions that flip state on each press
 * 
 * Usage:
 *   // Initialize with bindings:
 *   InputManager::GetInst().Initialize();  // Or set up custom bindings
 *   
 *   // In game loop:
 *   InputManager::GetInst().Update();
 *   
 *   // Check raw keys:
 *   if (InputManager::GetInst().IsKeyPressed(Key::SPACE)) { jump(); }
 *   
 *   // Check actions:
 *   if (InputManager::GetInst().IsActionHeld("move_forward")) { moveForward(); }
 */
class InputManager {
public:
    // Singleton accessor
    static InputManager& GetInst() {
        static InputManager instance;
        return instance;
    }

    // Delete copy/move
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    void Initialize();
    void Update();
    
    // ========================================================================
    // Key State Queries
    // ========================================================================
    
    bool IsKeyPressed(Key key) const;
    bool IsKeyHeld(Key key) const;
    bool IsKeyReleased(Key key) const;
    bool IsKeyDown(Key key) const;
    InputState GetKeyState(Key key) const;
    
    // ========================================================================
    // Action Binding
    // ========================================================================
    
    void BindKey(Key key, const std::string& action, bool isToggle = false);
    void UnbindKey(Key key);
    void UnbindAction(const std::string& action);
    void ClearBindings();
    
    // ========================================================================
    // Action State Queries
    // ========================================================================
    
    bool IsActionPressed(const std::string& action) const;
    bool IsActionHeld(const std::string& action) const;
    bool IsActionReleased(const std::string& action) const;
    bool GetToggleState(const std::string& action) const;
    void SetToggleState(const std::string& action, bool state);
    
    // ========================================================================
    // Utility
    // ========================================================================

    void ClearInputState();
    Key GetKeyForAction(const std::string& action) const;
    std::string GetActionForKey(Key key) const;
    void SetMouseSensitivity(float sensitivity);
    float GetMouseSensitivity() const;

private:
    InputManager();
    ~InputManager();

    // PIMPL pattern for platform-specific implementation
    class InputManagerImpl;
    std::unique_ptr<InputManagerImpl> pImpl;
    
    // Key state tracking
    std::unordered_map<Key, InputState> keyStates;
    std::unordered_map<Key, InputState> previousKeyStates;
    
    // Action bindings
    std::unordered_map<Key, KeyBinding> keyBindings;
    std::unordered_map<std::string, Key> actionToKey;
    std::unordered_map<std::string, bool> toggleStates;  // For toggle actions
    
    // Settings
    float mouseSensitivity = 80.0f;  // Default sensitivity
    
    // Helper functions
    InputState CalculateKeyState(Key key) const;
    void UpdateToggleStates();
};

} // namespace ASCIIgL
