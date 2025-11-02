#include <ASCIIgL/renderer/ScreenWinImpl.hpp>

#include <algorithm>
#include <string>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iterator>
#include <codecvt>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/MathUtil.hpp>

#include <ASCIIgL/engine/FPSClock.hpp>

#include <ASCIIgL/renderer/Screen.hpp>

// vendor
#include <nlohmann/json.hpp>

// Static member definitions for platform-specific implementations
#ifdef _WIN32

ScreenWinImpl::ScreenWinImpl(Screen& screenRef) 
    : screen(screenRef) {
}

// ScreenWinImpl method implementations (Unified Windows console implementation)
int ScreenWinImpl::Initialize(const unsigned int width, const unsigned int height, const unsigned int fontSize, const Palette& palette) {
    // First, get current console handle to check maximum window size with proper font
    HANDLE currentHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (currentHandle == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to get standard output handle.");
        return -1;
    }

    // Set the font FIRST to get accurate maximum window size calculations
    Logger::Debug(L"Setting font for accurate size calculations.");
    if (IsTerminal()) {
        SetFontTerminal(currentHandle, fontSize);
    }
    else {
        SetFontConsole(currentHandle, fontSize);
    }

    // Small delay to ensure font change takes effect
    Sleep(300);

    // NOW get the console screen buffer info with the correct font applied
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(currentHandle, &csbi);

    Logger::Info(L"Max console window size: " + std::to_wstring(csbi.dwMaximumWindowSize.X) + L"x" + std::to_wstring(csbi.dwMaximumWindowSize.Y));

    // Adjust dimensions to fit within maximum window size
    unsigned int adjustedWidth = width;
    unsigned int adjustedHeight = height;
    bool dimensionsAdjusted = false;

    if (height > csbi.dwMaximumWindowSize.Y) {
        adjustedHeight = csbi.dwMaximumWindowSize.Y;
        dimensionsAdjusted = true;
        Logger::Info(L"Requested height " + std::to_wstring(height) + 
                       L" exceeds maximum, adjusting to " + std::to_wstring(adjustedHeight));
    }
    if (width > csbi.dwMaximumWindowSize.X) {
        adjustedWidth = csbi.dwMaximumWindowSize.X;
        dimensionsAdjusted = true;
        Logger::Info(L"Requested screen width " + std::to_wstring(width) +
                       L" exceeds maximum, adjusting to " + std::to_wstring(adjustedWidth));
    }

    if (dimensionsAdjusted) {
        // Update instance screen dimensions
        screen._screen_width = adjustedWidth;
        screen._screen_height = adjustedHeight;
        Logger::Info(L"Screen dimensions automatically adjusted to " + 
                    std::to_wstring(adjustedWidth) + L"x" + std::to_wstring(adjustedHeight));
    }

    // NOW set up the coordinates with the correct dimensions
    dwBufferSize = COORD{ (SHORT)adjustedWidth, (SHORT)adjustedHeight };
    dwBufferCoord = COORD{ 0, 0 };
    rcRegion = SMALL_RECT{ 0, 0, SHORT(adjustedWidth - 1), SHORT(adjustedHeight - 1) };

    Logger::Debug(L"Creating console screen buffers.");
    _hOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (_hOutput == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to create console screen buffers.");
        return -1;
    }

    Logger::Debug(L"Setting buffer size.");
    SetConsoleScreenBufferSize(_hOutput, dwBufferSize);

    Logger::Debug(L"Setting console window info to minimal size.");
    SMALL_RECT m_rectWindow = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(_hOutput, TRUE, &m_rectWindow);

    // console vs terminal stuff
    if (IsTerminal()) {
        SetFontTerminal(_hOutput, fontSize);

        // set palette here
        SetPaletteTerminal(palette, _hOutput);

        // enabling VT processing for the terminal
        EnableVTMode();
    }
    else {
        SetFontConsole(_hOutput, fontSize);

        // set palette here
        SetPaletteConsole(palette, _hOutput);

        // setting console cursor to invisible for the console
        SetCursorInvisibleConsole(_hOutput);

        // disabling window resizing for the console
        DisableWindowResizingConsole();
    }

    // idsabling quick edit
    DWORD mode;
    GetConsoleMode(_hOutput, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    mode &= ~ENABLE_MOUSE_INPUT;
    SetConsoleMode(_hOutput, mode);

    CONSOLE_SCREEN_BUFFER_INFOEX info = { sizeof(info) };
    GetConsoleScreenBufferInfoEx(_hOutput, &info);
    info.srWindow = {0, 0, SHORT(dwBufferSize.X - 1), SHORT(dwBufferSize.Y - 1)};
    info.dwSize = dwBufferSize;
    info.dwMaximumWindowSize = dwBufferSize;
    SetConsoleScreenBufferInfoEx(_hOutput, &info);

    Logger::Debug(L"Setting physical size of console window.");
    SetConsoleWindowInfo(_hOutput, TRUE, &rcRegion);

    Logger::Debug(L"Setting _hOutput as the active console screen buffer.");
    SetConsoleActiveScreenBuffer(_hOutput);

    Logger::Debug(L"Deleting old buffers and creating new ones.");
	_pixelBuffer.resize(adjustedWidth * adjustedHeight);

    return 0;
}

void ScreenWinImpl::ClearPixelBuffer() {
    std::fill(_pixelBuffer.begin(), _pixelBuffer.end(), CHAR_INFO{L' ', 0x00});
}

void ScreenWinImpl::OutputBuffer() {
    WriteConsoleOutputW(_hOutput, _pixelBuffer.data(), dwBufferSize, dwBufferCoord, &rcRegion);
}

void ScreenWinImpl::RenderTabTitle() {
    char titleBuffer[256];

    sprintf_s(
        titleBuffer, sizeof(titleBuffer),
        "%ls",
        screen._title.c_str()
    );

    SetConsoleTitleA(titleBuffer);
}

void ScreenWinImpl::PlotPixel(const glm::vec2& p, const WCHAR character, const unsigned short Colour) {
    int x = static_cast<int>(p.x);  // No doubling for square fonts
    int y = static_cast<int>(p.y);
    if (x >= 0 && x < static_cast<int>(screen._screen_width) && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot single pixel for square fonts
        _pixelBuffer[y * screen._screen_width + x].Char.UnicodeChar = character;
        _pixelBuffer[y * screen._screen_width + x].Attributes = static_cast<WORD>(Colour);
    }
}

void ScreenWinImpl::PlotPixel(const glm::vec2& p, const CHAR_INFO charCol) {
    int x = static_cast<int>(p.x);  // No doubling for square fonts
    int y = static_cast<int>(p.y);
    if (x >= 0 && x < static_cast<int>(screen._screen_width) && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot single pixel for square fonts
        _pixelBuffer[y * screen._screen_width + x] = charCol;
    }
}

void ScreenWinImpl::PlotPixel(int x, int y, WCHAR character, const unsigned short Colour) {
    // No doubling for square fonts
    if (x >= 0 && x < static_cast<int>(screen._screen_width) && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot single pixel for square fonts
        _pixelBuffer[y * screen._screen_width + x].Char.UnicodeChar = character;
        _pixelBuffer[y * screen._screen_width + x].Attributes = static_cast<WORD>(Colour);
    }
}

void ScreenWinImpl::PlotPixel(int x, int y, CHAR_INFO charCol) {
    // No doubling for square fonts
    if (x >= 0 && x < static_cast<int>(screen._screen_width) && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot single pixel for square fonts
        _pixelBuffer[y * screen._screen_width + x] = charCol;
    }
}

void ScreenWinImpl::PlotPixel(int idx, const CHAR_INFO charCol) {
    // No doubling for square fonts
    if (idx >= 0 && idx < static_cast<int>(screen._screen_width * screen._screen_height)) {
        _pixelBuffer[idx] = charCol;
    }
}

std::vector<CHAR_INFO>& ScreenWinImpl::GetPixelBuffer() {
    return _pixelBuffer;
}

bool ScreenWinImpl::IsTerminal() {
    // Method 1: Check for Windows Terminal specific environment variables
    const char* wtSession = std::getenv("WT_SESSION");
    if (wtSession != nullptr) {
        return true;
    }
    
    // Method 2: Check for TERM_PROGRAM environment variable (modern terminals set this)
    const char* termProgram = std::getenv("TERM_PROGRAM");
    if (termProgram != nullptr && std::string(termProgram) == "vscode") {
        // VSCode integrated terminal (which uses Windows Terminal backend)
        return true;
    }

    // Method 3: Check if ConEmu environment variable exists (ConEmu is another modern terminal)
    const char* conEmuANSI = std::getenv("ConEmuANSI");
    if (conEmuANSI != nullptr) {
        return true; // ConEmu supports modern terminal features
    }
    
    // If none of the above conditions are met, assume it's traditional Command Prompt
    return false;
}


void ScreenWinImpl::SetFontConsole(HANDLE currentHandle, unsigned int fontSize) {
    CONSOLE_FONT_INFOEX fontForSizing;
    fontForSizing.cbSize = sizeof(fontForSizing);
    fontForSizing.nFont = 0;
    fontForSizing.dwFontSize.X = fontSize;
    fontForSizing.dwFontSize.Y = fontSize;
    fontForSizing.FontFamily = FF_DONTCARE;
    fontForSizing.FontWeight = FW_NORMAL;
    wcscpy_s(fontForSizing.FaceName, LF_FACESIZE, L"Lucida Console");
    if (!SetCurrentConsoleFontEx(currentHandle, FALSE, &fontForSizing)) {
        Logger::Error(L"Failed to set font.");
    }
}

void ScreenWinImpl::SetFontTerminal(HANDLE currentHandle, unsigned int fontSize) {
    Logger::Info(L"Attempting to modify Windows Terminal settings.json file directly.");
    
    // Convert pixel-based font size to Windows Terminal point size
    float terminalFontSize = ConvertPixelSizeToTerminalPoints(fontSize);
    Logger::Debug(L"Converting pixel size " + std::to_wstring(fontSize) + 
                  L" to terminal point size " + std::to_wstring(terminalFontSize));

    std::wstring settingsPath = GetTerminalSettingsPath();
    if (!settingsPath.empty() && ModifyTerminalFont(settingsPath, terminalFontSize)) {
        Logger::Info(L"Successfully modified Windows Terminal settings.json");
    }
}

float ScreenWinImpl::ConvertPixelSizeToTerminalPoints(unsigned int pixelSize) {
    // Precise conversion from pixel-based font size to typographic points
    // 
    // Method: Get system DPI and calculate the actual point size that would 
    // produce the same pixel height at the current DPI setting
    
    // Get the DPI of the primary display
    HDC hdc = GetDC(NULL);
    if (hdc == NULL) {
        Logger::Warning(L"Could not get device context for DPI calculation, using default conversion");
        return static_cast<float>(pixelSize) * 0.75f;  // Fallback to empirical conversion
    }
    
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);  // Vertical DPI (typically 96 or 120)
    ReleaseDC(NULL, hdc);
    
    // Typographic conversion: 1 point = 1/72 inch
    // At 96 DPI: 1 point = 96/72 = 1.333... pixels
    // At 120 DPI: 1 point = 120/72 = 1.666... pixels
    // 
    // To convert pixels to points: points = pixels * 72 / DPI
    float pointSize = static_cast<float>(pixelSize) * 72.0f / static_cast<float>(dpiY);
    
    // Apply a small adjustment factor because Windows Terminal tends to render
    // fonts slightly differently than the legacy console due to different 
    // rendering engines (DirectWrite vs GDI)
    const float renderingAdjustment = 0.9f;  // Fine-tune based on visual testing
    pointSize *= renderingAdjustment;
    
    Logger::Debug(L"DPI: " + std::to_wstring(dpiY) + 
                  L", Pixel size: " + std::to_wstring(pixelSize) + 
                  L", Calculated point size: " + std::to_wstring(pointSize));

    return std::max(1.0f, pointSize);
}

std::wstring ScreenWinImpl::GetTerminalSettingsPath() {
    // Windows Terminal settings are stored in:
    // %LOCALAPPDATA%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json
    
    wchar_t* localAppData = nullptr;
    size_t len = 0;
    errno_t err = _wdupenv_s(&localAppData, &len, L"LOCALAPPDATA");
    
    if (err != 0 || localAppData == nullptr) {
        Logger::Error(L"Could not get LOCALAPPDATA environment variable.");
        return L"";
    }
    
    std::wstring settingsPath = localAppData;
    free(localAppData);
    
    // Try different possible Windows Terminal package names
    std::vector<std::wstring> possiblePaths = {
        settingsPath + L"\\Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\LocalState\\settings.json",
        settingsPath + L"\\Packages\\Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe\\LocalState\\settings.json",
        settingsPath + L"\\Microsoft\\Windows Terminal\\settings.json"
    };
    
    for (const auto& path : possiblePaths) {
        if (std::filesystem::exists(path)) {
            Logger::Info(L"Found Windows Terminal settings at: " + path);
            return path;
        }
    }
    
    Logger::Info(L"Windows Terminal settings.json not found in standard locations.");
    return L"";
}

bool ScreenWinImpl::ModifyTerminalFont(const std::wstring& settingsPath, float fontSize) {             
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    try {
        std::string settingsPathStr = converter.to_bytes(settingsPath);

        // Read file as UTF-8
        std::ifstream file(settingsPathStr);
        if (!file.is_open()) {
            Logger::Error(L"Could not open Windows Terminal settings file.");
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Create backup
        std::filesystem::copy_file(settingsPath, settingsPath + L".backup", std::filesystem::copy_options::overwrite_existing);

        // Parse JSON
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(content);
        } catch (const std::exception& e) {
            Logger::Error(L"Failed to parse Windows Terminal settings.json: " + std::wstring(converter.from_bytes(e.what())));
            return false;
        }

        // Ensure font is installed
        if (!IsFontInstalled(L"Square")) {
            std::wstring fontPath = L"res\\ASCIIgL\\fonts\\square\\square.ttf";
            if (InstallFontFromFile(fontPath)) {
                Logger::Info(L"Successfully installed 'Square' font.");
            } else {
                Logger::Warning(L"Failed to install 'Square' font. Font face setting may not apply correctly.");
            }
        }

        // Only update font in profiles.defaults, not at the root level
        if (j.contains("profiles") && j["profiles"].is_object()) {
            nlohmann::json& profiles = j["profiles"];
            if (!profiles.contains("defaults") || !profiles["defaults"].is_object()) {
                profiles["defaults"] = nlohmann::json::object();
            }
            nlohmann::json& defaults = profiles["defaults"];
            if (!defaults.contains("font") || !defaults["font"].is_object()) {
                defaults["font"] = nlohmann::json::object();
            }
            nlohmann::json& fontObj = defaults["font"];
            fontObj["size"] = fontSize;
            fontObj["face"] = "Square Modern";
            fontObj["lineHeight"] = 1.0;
        } else {
            Logger::Error(L"Could not find 'profiles' object in settings.json.");
            return false;
        }

        // Write back as UTF-8
        std::ofstream outFile(settingsPathStr);
        if (!outFile.is_open()) return false;
        outFile << j.dump(4);
        outFile.close();

        Logger::Debug(L"Modified Windows Terminal settings");
        return true;

    } catch (const std::exception& e) {
        Logger::Error(L"Failed to modify Windows Terminal settings: " + std::wstring(converter.from_bytes(e.what())));
        return false;
    }
}

bool ScreenWinImpl::IsFontInstalled(const std::wstring& fontName) {
    // Check if font is installed by enumerating fonts
    HDC hdc = GetDC(NULL);
    if (hdc == NULL) return false;
    
    bool found = false;
    
    // Static callback function for font enumeration
    struct FontCheckData {
        std::wstring targetFont;
        bool* foundPtr;
    };
    
    FontCheckData checkData = { fontName, &found };
    
    // Static callback function (required for Windows API)
    static auto enumCallback = [](const LOGFONTW* lf, const TEXTMETRICW* tm, DWORD fontType, LPARAM lParam) -> int {
        FontCheckData* data = reinterpret_cast<FontCheckData*>(lParam);
        std::wstring currentFont(lf->lfFaceName);
        
        if (currentFont == data->targetFont) {
            *(data->foundPtr) = true;
            return 0; // Stop enumeration
        }
        return 1; // Continue enumeration
    };
    
    EnumFontFamiliesW(hdc, fontName.c_str(), (FONTENUMPROCW)enumCallback, (LPARAM)&checkData);
    
    ReleaseDC(NULL, hdc);
    return found;
}

bool ScreenWinImpl::InstallFontFromFile(const std::wstring& fontPath) {
    // Check if font file exists
    if (!std::filesystem::exists(fontPath)) {
        Logger::Error(L"Font file not found: " + fontPath);
        return false;
    }
    
    Logger::Info(L"Attempting to install font: " + fontPath);
    
    // Try system-wide installation first (requires admin privileges)
    int systemResult = AddFontResourceExW(fontPath.c_str(), FR_NOT_ENUM, 0);
    if (systemResult > 0) {
        Logger::Info(L"Font installed system-wide: " + fontPath);
        SendMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
        Sleep(200); // Allow more time for system-wide registration
        return true;
    }
    
    Logger::Warning(L"Font may not be available to Windows Terminal. Try running as administrator for system-wide installation.");
    
    // Notify system of font change
    SendMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
    Sleep(200); // Give system time to process font registration
    
    // Verify font installation by checking if it's now available
    bool isInstalled = IsFontInstalled(L"Square");
    if (isInstalled) {
        Logger::Info(L"Font verification successful: Square is now available");
    } else {
        Logger::Warning(L"Font verification failed: Square may not be properly registered");
        Logger::Info(L"Suggestion: Restart the application or Windows Terminal, or install the font manually");
    }
    
    return true;
}

void ScreenWinImpl::SetCursorInvisibleConsole(HANDLE currentHandle) {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(currentHandle, &cursorInfo);
    cursorInfo.bVisible = FALSE; // Set cursor visibility to false
    SetConsoleCursorInfo(currentHandle, &cursorInfo);
}

void ScreenWinImpl::DisableWindowResizingConsole() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_MAXIMIZEBOX; // Disable maximize button
        style &= ~WS_SIZEBOX;     // Disable resizing border
        SetWindowLong(hwnd, GWL_STYLE, style);
    } else {
        Logger::Error(L"Failed to get console window handle for disabling resizing.");
    }
}

void ScreenWinImpl::SetPaletteTerminal(const Palette& palette, HANDLE& hOutput) {
    Logger::Info(L"Attempting to modify Windows Terminal color scheme.");

    std::wstring settingsPath = GetTerminalSettingsPath();
    if (settingsPath.empty()) {
        Logger::Error(L"Could not find Windows Terminal settings path.");
        return;
    }

    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    try {
        std::string settingsPathStr = converter.to_bytes(settingsPath);

        // Read file as UTF-8
        std::ifstream file(settingsPathStr);
        if (!file.is_open()) {
            Logger::Error(L"Could not open Windows Terminal settings file.");
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Create backup
        std::filesystem::copy_file(settingsPath, settingsPath + L".palette_backup", std::filesystem::copy_options::overwrite_existing);

        // Parse JSON
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(content);
        } catch (const std::exception& e) {
            Logger::Error(L"Failed to parse Windows Terminal settings.json: " + std::wstring(converter.from_bytes(e.what())));
            return;
        }

        // Build custom color scheme JSON object
        nlohmann::json customScheme;
        customScheme["name"] = "ASCIIgL Custom";
        
        // Set foreground, background, and cursor overrides to black (palette index 0)
        glm::ivec3 blackRGB = palette.GetRGB(0);
        int blackR = (blackRGB.r * 255) / 15;
        int blackG = (blackRGB.g * 255) / 15;
        int blackB = (blackRGB.b * 255) / 15;
        char blackHex[8];
        snprintf(blackHex, sizeof(blackHex), "#%02X%02X%02X", blackR, blackG, blackB);
        customScheme["foreground"] = blackHex;
        customScheme["background"] = blackHex;
        customScheme["cursorColor"] = blackHex;
        
        std::vector<std::string> colorNames = {
            "black", "blue", "green", "cyan", "red", "purple", "yellow", "white",
            "brightBlack", "brightBlue", "brightGreen", "brightCyan", "brightRed", "brightPurple", "brightYellow", "brightWhite"
        };
        for (size_t i = 0; i < std::min(static_cast<size_t>(Palette::COLOR_COUNT), colorNames.size()); ++i) {
            glm::ivec3 rgb = palette.GetRGB(static_cast<uint8_t>(i));
            // Convert from 0-15 to 0-255 range: (value * 255) / 15
            int r = (rgb.r * 255) / 15;
            int g = (rgb.g * 255) / 15;
            int b = (rgb.b * 255) / 15;
            char hexColor[8];
            snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X", r, g, b);
            customScheme[colorNames[i]] = hexColor;
        }

        // Ensure "schemes" exists and is an array
        if (!j.contains("schemes") || !j["schemes"].is_array()) {
            j["schemes"] = nlohmann::json::array();
        }

        // Remove any existing "ASCIIgL Custom" scheme
        auto& schemes = j["schemes"];
        schemes.erase(std::remove_if(schemes.begin(), schemes.end(), [](const nlohmann::json& scheme) -> bool {
            return scheme.contains("name") && scheme["name"] == "ASCIIgL Custom";
        }), schemes.end());

        // Add the new custom scheme
        schemes.push_back(customScheme);

        // Write back as UTF-8
        std::ofstream outFile(settingsPathStr);
        if (!outFile.is_open()) {
            Logger::Error(L"Could not write to Windows Terminal settings file.");
            return;
        }
        outFile << j.dump(4); // Pretty print with 4-space indent
        outFile.close();

        Logger::Info(L"Successfully modified Windows Terminal color scheme.");

    } catch (const std::exception& e) {
        Logger::Error(L"Failed to modify Windows Terminal color scheme: " + std::wstring(converter.from_bytes(e.what())));
    }
}

void ScreenWinImpl::SetPaletteConsole(const Palette& palette, HANDLE& hOutput) {
    Logger::Info(L"Setting console palette for legacy Command Prompt.");

    if (hOutput == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to get console output handle for palette setting.");
        return;
    }
    
    // Get current console screen buffer info
    CONSOLE_SCREEN_BUFFER_INFOEX csbi;
    csbi.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    
    if (!GetConsoleScreenBufferInfoEx(hOutput, &csbi)) {
        Logger::Error(L"Failed to get console screen buffer info for palette setting.");
        return;
    }
    
    // Map the 16 palette colors to the console color table
    for (uint8_t i = 0; i < Palette::COLOR_COUNT; ++i) {
        glm::ivec3 rgb = palette.GetRGB(i);
        
        // Convert RGB from 0-15 to 0-255 range: (value * 255) / 15
        BYTE r = static_cast<BYTE>((rgb.r * 255) / 15);
        BYTE g = static_cast<BYTE>((rgb.g * 255) / 15);
        BYTE b = static_cast<BYTE>((rgb.b * 255) / 15);
        
        // Set the color in the console color table
        csbi.ColorTable[i] = RGB(r, g, b);
    }
    
    // Apply the updated color table to the console
    if (!SetConsoleScreenBufferInfoEx(hOutput, &csbi)) {
        Logger::Error(L"Failed to set console screen buffer info for palette.");
        return;
    }
    
    Logger::Info(L"Successfully set console palette for legacy Command Prompt.");
}

void ScreenWinImpl::EnableVTMode() {
    // Enable Virtual Terminal Processing for ANSI escape codes
    if (_hOutput == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to get standard output handle for VT mode.");
        return;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(_hOutput, &dwMode)) {
        Logger::Error(L"Failed to get console mode for VT mode.");
        return;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(_hOutput, dwMode)) {
        Logger::Error(L"Failed to set console mode for VT mode.");
        return;
    }

    Logger::Info(L"Enabled Virtual Terminal Processing mode.");
}

#endif