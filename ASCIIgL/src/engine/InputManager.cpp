#include <ASCIIgL/engine/InputManager.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/util/CoverageJson.hpp>
#include <algorithm>
#include <cstring>
#include <vector>

// Platform-specific implementations
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef OEMRESOURCE
        #define OEMRESOURCE
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

        HWND ResolveHostHwnd() const {
            auto& screen = Screen::GetInst();
            if (!screen.IsRenderToTerminal()) {
                return static_cast<HWND>(screen.GetWindowHandle());
            }
            HWND console = GetConsoleWindow();
            HWND fg = GetForegroundWindow();
            if (fg && fg == console) {
                return console;
            }
            // Windows Terminal / ConPTY: console HWND is not the visible window.
            return fg ? fg : console;
        }

        bool IsHostFocused() const {
            HWND fg = GetForegroundWindow();
            if (!fg) {
                return false;
            }

            auto& screen = Screen::GetInst();
            if (!screen.IsRenderToTerminal()) {
                HWND hwnd = static_cast<HWND>(screen.GetWindowHandle());
                return hwnd && hwnd == fg;
            }

            HWND console = GetConsoleWindow();
            if (fg == console) {
                return true;
            }

            wchar_t className[256] = {};
            if (GetClassNameW(fg, className, 256) > 0) {
                if (wcsstr(className, L"CASCADIA") != nullptr ||
                    wcsstr(className, L"ConsoleWindowClass") != nullptr) {
                    return true;
                }
            }
            return false;
        }

        bool GetClientCenterScreen(POINT& outCenter) const {
            HWND hwnd = ResolveHostHwnd();
            if (!hwnd) {
                return false;
            }
            RECT client{};
            if (!GetClientRect(hwnd, &client)) {
                return false;
            }
            POINT center{
                (client.left + client.right) / 2,
                (client.top + client.bottom) / 2
            };
            if (!ClientToScreen(hwnd, &center)) {
                return false;
            }
            outCenter = center;
            return true;
        }

        bool GetPointerClientPos(float& outX, float& outY) const {
            HWND hwnd = ResolveHostHwnd();
            if (!hwnd) {
                return false;
            }
            POINT cursor{};
            if (!GetCursorPos(&cursor)) {
                return false;
            }
            if (!ScreenToClient(hwnd, &cursor)) {
                return false;
            }
            outX = static_cast<float>(cursor.x);
            outY = static_cast<float>(cursor.y);
            return true;
        }

        bool GetClientSize(int& outW, int& outH) const {
            HWND hwnd = ResolveHostHwnd();
            if (!hwnd) {
                return false;
            }
            RECT client{};
            if (!GetClientRect(hwnd, &client)) {
                return false;
            }
            outW = client.right - client.left;
            outH = client.bottom - client.top;
            return outW > 0 && outH > 0;
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

        bool IsHostFocused() const { return false; }

        bool GetPointerClientPos(float& outX, float& outY) const {
            outX = 0.0f;
            outY = 0.0f;
            return false;
        }

        bool GetClientSize(int& outW, int& outH) const {
            outW = 0;
            outH = 0;
            return false;
        }
    };

} // namespace ASCIIgL

#endif

namespace ASCIIgL {

namespace {

#ifdef _WIN32
HCURSOR g_savedArrowCursor = nullptr;
bool g_systemCursorHidden = false;
#endif

} // namespace

// ============================================================================
// InputManager Implementation
// ============================================================================

InputManager::InputManager() 
    : pImpl(std::make_unique<InputManagerImpl>()) {
}

InputManager::~InputManager() {
    Shutdown();
}

void InputManager::Shutdown() {
    SetMouseCapture(false);
    SetTerminalMouseTracking(false);
}

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
    BindKey(Key::Q, "drop_item");
    BindKey(Key::F, "interact_left");
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
    UpdateMouseButtons();
    PollConsoleMouseInput();
    UpdateMouse();
}

void InputManager::UpdateMouseButtons() {
#ifdef _WIN32
    previousMouseButtonStates[0] = mouseButtonStates[0];
    previousMouseButtonStates[1] = mouseButtonStates[1];

    const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool down[2] = {leftDown, rightDown};

    for (int i = 0; i < 2; ++i) {
        const bool wasDown =
            previousMouseButtonStates[i] == InputState::Pressed ||
            previousMouseButtonStates[i] == InputState::Held;
        if (down[i] && !wasDown) {
            mouseButtonStates[i] = InputState::Pressed;
        } else if (down[i] && wasDown) {
            mouseButtonStates[i] = InputState::Held;
        } else {
            mouseButtonStates[i] = InputState::Released;
        }
    }
#else
    previousMouseButtonStates[0] = mouseButtonStates[0] = InputState::Released;
    previousMouseButtonStates[1] = mouseButtonStates[1] = InputState::Released;
#endif
}

int InputManager::MouseButtonIndex(MouseButton button) {
    return button == MouseButton::Right ? 1 : 0;
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const {
    return mouseButtonStates[MouseButtonIndex(button)] == InputState::Pressed;
}

bool InputManager::IsMouseButtonHeld(MouseButton button) const {
    return mouseButtonStates[MouseButtonIndex(button)] == InputState::Held;
}

bool InputManager::IsMouseButtonDown(MouseButton button) const {
    const InputState state = mouseButtonStates[MouseButtonIndex(button)];
    return state == InputState::Pressed || state == InputState::Held;
}

void InputManager::AddScrollDelta(int wheelDelta) {
    scrollDeltaAccum += wheelDelta;
}

int InputManager::ConsumeScrollDelta() {
#ifdef _WIN32
    constexpr int kWheelDelta = 120; // WHEEL_DELTA
#else
    constexpr int kWheelDelta = 120;
#endif
    if (scrollDeltaAccum == 0 || kWheelDelta == 0) {
        return 0;
    }
    const int notches = scrollDeltaAccum / kWheelDelta;
    scrollDeltaAccum -= notches * kWheelDelta;
    return notches;
}

void InputManager::PollConsoleMouseInput() {
#ifdef _WIN32
    if (!terminalMouseTracking || !Screen::GetInst().IsRenderToTerminal()) {
        return;
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hIn == nullptr) {
        return;
    }

    DWORD numEvents = 0;
    if (!GetNumberOfConsoleInputEvents(hIn, &numEvents) || numEvents == 0) {
        return;
    }

    // Drain console input: keep wheel + cell position; discard keys (GetAsyncKeyState).
    for (DWORD i = 0; i < numEvents; ++i) {
        INPUT_RECORD record{};
        DWORD read = 0;
        if (!ReadConsoleInput(hIn, &record, 1, &read) || read == 0) {
            break;
        }
        if (record.EventType != MOUSE_EVENT) {
            continue;
        }
        const MOUSE_EVENT_RECORD& mouse = record.Event.MouseEvent;

        // Console reports the character cell under the cursor (matches the text grid,
        // not the full Windows Terminal chrome client rect).
        consolePointerCell = {
            static_cast<float>(mouse.dwMousePosition.X) + 0.5f,
            static_cast<float>(mouse.dwMousePosition.Y) + 0.5f
        };
        consolePointerValid = true;

        if (mouse.dwEventFlags & MOUSE_WHEELED) {
            const SHORT delta = static_cast<SHORT>(HIWORD(mouse.dwButtonState));
            AddScrollDelta(static_cast<int>(delta));
        }
    }
#endif
}

void InputManager::ApplyTerminalMouseInputMode(bool enabled) {
#ifdef _WIN32
    if (!Screen::GetInst().IsRenderToTerminal()) {
        return;
    }

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hIn == nullptr) {
        return;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(hIn, &mode)) {
        return;
    }

    if (enabled) {
        if (!savedInputConsoleModeValid) {
            savedInputConsoleMode = mode;
            savedInputConsoleModeValid = true;
        }
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(hIn, mode);
    } else if (savedInputConsoleModeValid) {
        SetConsoleMode(hIn, savedInputConsoleMode);
        savedInputConsoleModeValid = false;
    }
#else
    (void)enabled;
#endif
}

void InputManager::UpdateMouse() {
#ifdef _WIN32
    mouseDeltaAccum = {0.0f, 0.0f};

    if (!mouseCaptured || !pImpl->IsHostFocused()) {
        return;
    }

    POINT center{};
    if (!pImpl->GetClientCenterScreen(center)) {
        return;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }

    const float dx = static_cast<float>(cursor.x - center.x);
    const float dy = static_cast<float>(cursor.y - center.y);

    if (ignoreNextMouseSample) {
        ignoreNextMouseSample = false;
    } else {
        mouseDeltaAccum = {dx, dy};
    }

    SetCursorPos(center.x, center.y);
#else
    mouseDeltaAccum = {0.0f, 0.0f};
#endif
}

void InputManager::ApplySystemCursorHidden(bool hidden) {
#ifdef _WIN32
    if (hidden == g_systemCursorHidden) {
        return;
    }

    if (hidden) {
        if (!g_savedArrowCursor) {
            HCURSOR arrow = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
            if (arrow) {
                g_savedArrowCursor = CopyCursor(arrow);
            }
        }

        // Invisible 32x32 cursor: AND all 1s, XOR all 0s.
        std::vector<BYTE> andPlane(128, 0xFF);
        std::vector<BYTE> xorPlane(128, 0x00);
        HCURSOR blank = CreateCursor(nullptr, 0, 0, 32, 32, andPlane.data(), xorPlane.data());
        if (blank) {
            // SetSystemCursor takes ownership and destroys the handle.
            SetSystemCursor(blank, OCR_NORMAL);
            g_systemCursorHidden = true;
        }
    } else {
        if (g_savedArrowCursor) {
            HCURSOR restore = CopyCursor(g_savedArrowCursor);
            if (restore) {
                SetSystemCursor(restore, OCR_NORMAL);
            }
        } else {
            SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
        }
        g_systemCursorHidden = false;
    }
#else
    (void)hidden;
#endif
}

void InputManager::WriteTerminalMouseTracking(bool enabled) {
#ifdef _WIN32
    if (!Screen::GetInst().IsRenderToTerminal()) {
        return;
    }

    // Any-event tracking + SGR encoding. Suppresses WT selection while active.
    const char* seq = enabled
        ? "\x1b[?1003h\x1b[?1006h"
        : "\x1b[?1003l\x1b[?1006l\x1b[?1000l";
    Screen::GetInst().WriteOutputBytes(seq, std::strlen(seq));
#else
    (void)enabled;
#endif
}

void InputManager::SetMouseCapture(bool capture) {
    if (mouseCaptured == capture) {
        if (capture) {
            // Re-assert hidden cursor if already captured.
            ApplySystemCursorHidden(true);
        }
        return;
    }

    mouseCaptured = capture;
    mouseDeltaAccum = {0.0f, 0.0f};

    if (capture) {
        ApplySystemCursorHidden(true);
        ignoreNextMouseSample = true;
#ifdef _WIN32
        POINT center{};
        if (pImpl->GetClientCenterScreen(center)) {
            SetCursorPos(center.x, center.y);
        }
#endif
    } else {
        ApplySystemCursorHidden(false);
        ignoreNextMouseSample = false;
    }
}

bool InputManager::IsMouseCaptured() const {
    return mouseCaptured;
}

void InputManager::SetTerminalMouseTracking(bool enabled) {
    if (terminalMouseTracking == enabled) {
        return;
    }
    terminalMouseTracking = enabled;
    if (!enabled) {
        consolePointerValid = false;
    }
    WriteTerminalMouseTracking(enabled);
    ApplyTerminalMouseInputMode(enabled);
}

bool InputManager::IsTerminalMouseTrackingEnabled() const {
    return terminalMouseTracking;
}

glm::vec2 InputManager::GetMouseDelta() {
    const glm::vec2 delta = mouseDeltaAccum;
    mouseDeltaAccum = {0.0f, 0.0f};
    return delta;
}

glm::vec2 InputManager::GetPointerClientPos() const {
    float x = 0.0f;
    float y = 0.0f;
    if (!pImpl->GetPointerClientPos(x, y)) {
        return {0.0f, 0.0f};
    }
    return {x, y};
}

glm::vec2 InputManager::GetPointerScreenPos() const {
    auto& screen = Screen::GetInst();
    const float screenW = static_cast<float>(screen.GetWidth());
    const float screenH = static_cast<float>(screen.GetHeight());
    if (screenW <= 0.0f || screenH <= 0.0f) {
        return {0.0f, 0.0f};
    }

    // Terminal: prefer console MOUSE_EVENT cell coords. GetCursorPos against the WT
    // HWND client includes tabs/title chrome and systematically misses the text grid.
    if (screen.IsRenderToTerminal() && consolePointerValid) {
        return {
            std::clamp(consolePointerCell.x, 0.0f, screenW - 0.001f),
            std::clamp(consolePointerCell.y, 0.0f, screenH - 0.001f)
        };
    }

    float cx = 0.0f;
    float cy = 0.0f;
    if (!pImpl->GetPointerClientPos(cx, cy)) {
        return {0.0f, 0.0f};
    }

    // Window ASCII pass maps backbuffer pixels → cells via fixed cell pixel size
    // (pix / cellPixels), not by stretching the grid to the client. Match that.
    if (!screen.IsRenderToTerminal()) {
        int cellPixelsX = 0;
        int cellPixelsY = 0;
        if (CoverageJson::GetCellSizeForFontSize(screen.GetFontSize(), &cellPixelsX, &cellPixelsY)
            && cellPixelsX > 0 && cellPixelsY > 0) {
            return {
                std::clamp(cx / static_cast<float>(cellPixelsX), 0.0f, screenW - 0.001f),
                std::clamp(cy / static_cast<float>(cellPixelsY), 0.0f, screenH - 0.001f)
            };
        }
    }

    // Fallback stretch (terminal before first MOUSE_EVENT, or missing coverage data).
    int clientW = 0;
    int clientH = 0;
    if (!pImpl->GetClientSize(clientW, clientH) || clientW <= 0 || clientH <= 0) {
        return {0.0f, 0.0f};
    }

    return {
        std::clamp((cx / static_cast<float>(clientW)) * screenW, 0.0f, screenW - 0.001f),
        std::clamp((cy / static_cast<float>(clientH)) * screenH, 0.0f, screenH - 0.001f)
    };
}

bool InputManager::IsHostFocused() const {
    return pImpl->IsHostFocused();
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
    mouseButtonStates[0] = mouseButtonStates[1] = InputState::Released;
    previousMouseButtonStates[0] = previousMouseButtonStates[1] = InputState::Released;
    mouseDeltaAccum = {0.0f, 0.0f};
    scrollDeltaAccum = 0;
    consolePointerValid = false;
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
