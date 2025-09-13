#include <ASCIIgL/renderer/Screen.hpp>

#include <thread>
#include <chrono>
#include <algorithm>
#include <deque>
#include <string>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <regex>
#include <locale>
#include <codecvt>

#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/util/MathUtil.hpp>

// Platform-specific implementation
#ifdef _WIN32
    // Windows API
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <Windows.h>

    // Windows-specific implementation classes
    
    // Unified Windows implementation for both Command Prompt and Windows Terminal
    class Screen::WindowsImpl {
    private:
        Screen& screen;  // Reference to the Screen instance
    
    public:
        // Console handles and coordinate structures
        HANDLE _hOutput = nullptr;
        COORD dwBufferSize = {0, 0};
        COORD dwBufferCoord = {0, 0};
        SMALL_RECT rcRegion = {0, 0, 0, 0};

        // Buffers for Windows console output
        CHAR_INFO* pixelBuffer = nullptr;

        // Constructor takes Screen reference
        WindowsImpl(Screen& screenRef) : screen(screenRef) {}

        // Destructor
        ~WindowsImpl() {
            if (pixelBuffer) {
                delete[] pixelBuffer;
                pixelBuffer = nullptr;
            }
        }

        // Implementation methods
        int Initialize(const unsigned int true_width, const unsigned int height, const unsigned int fontSize);
        void ClearBuffer();
        void OutputBuffer();
        void RenderTitle(bool showFps);
        void PlotPixel(glm::vec2 p, char character, short Colour);
        void PlotPixel(glm::vec2 p, CHAR_INFO charCol);
        void PlotPixel(int x, int y, char character, short Colour);
        void PlotPixel(int x, int y, CHAR_INFO charCol);
        CHAR_INFO* GetPixelBuffer();
        bool IsWindowsTerminal();
        std::wstring GetWindowsTerminalSettingsPath();
        bool ModifyWindowsTerminalFont(const std::wstring& settingsPath, float fontSize);
        float ConvertPixelSizeToTerminalPoints(unsigned int pixelSize);
        bool IsFontInstalled(const std::wstring& fontName);
        bool InstallFontFromFile(const std::wstring& fontFilePath);
    };

#else
    // No generic implementation is needed yet, only developing on windows right now
#endif

// Static member definitions for platform-specific implementations
#ifdef _WIN32

// WindowsImpl method implementations (Unified Windows console implementation)
int Screen::WindowsImpl::Initialize(const unsigned int true_width, const unsigned int height, const unsigned int fontSize) {
    // First, get current console handle to check maximum window size with proper font
    HANDLE currentHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (currentHandle == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to get standard output handle.");
        return SCREEN_WIN_BUFFER_CREATION_FAILED;
    }

    // Set the font FIRST to get accurate maximum window size calculations
    Logger::Debug(L"Setting font for accurate size calculations.");
    if (IsWindowsTerminal()) {
        screen.SetFontModernTerminal(currentHandle, fontSize);
    }
    else {
        screen.SetFontConsole(currentHandle, fontSize);
    }

    // Small delay to ensure font change takes effect
    Sleep(100);

    // NOW get the console screen buffer info with the correct font applied
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(currentHandle, &csbi);

    Logger::Debug(L"Max console window size: " + std::to_wstring(csbi.dwMaximumWindowSize.X) + L"x" + std::to_wstring(csbi.dwMaximumWindowSize.Y));

    // Adjust dimensions to fit within maximum window size
    unsigned int adjustedWidth = true_width;
    unsigned int adjustedHeight = height;
    bool dimensionsAdjusted = false;

    if (height > csbi.dwMaximumWindowSize.Y) {
        adjustedHeight = csbi.dwMaximumWindowSize.Y;
        dimensionsAdjusted = true;
        Logger::Info(L"Requested height " + std::to_wstring(height) + 
                       L" exceeds maximum, adjusting to " + std::to_wstring(adjustedHeight));
    }
    if (true_width > csbi.dwMaximumWindowSize.X) {
        adjustedWidth = csbi.dwMaximumWindowSize.X;
        dimensionsAdjusted = true;
        Logger::Info(L"Requested true width " + std::to_wstring(true_width) +
                       L" exceeds maximum, adjusting to " + std::to_wstring(adjustedWidth));
    }

    if (dimensionsAdjusted) {
        // Update instance screen dimensions
        screen._true_screen_width = MathUtil::FloorToEven(adjustedWidth);
        screen._visible_screen_width = adjustedWidth / 2;
        screen._screen_height = adjustedHeight;
        Logger::Info(L"Screen dimensions automatically adjusted to " + 
                    std::to_wstring(adjustedWidth) + L"x" + std::to_wstring(adjustedHeight));
    }

    // NOW set up the coordinates with the correct dimensions
    dwBufferSize = COORD{ (SHORT)adjustedWidth, (SHORT)adjustedHeight };
    dwBufferCoord = COORD{ 0, 0 };
    rcRegion = SMALL_RECT{ 0, 0, SHORT(adjustedWidth - 1), SHORT(adjustedHeight - 1) };

    Logger::Debug(L"Creating front and back console screen buffers for double buffering.");
    _hOutput = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (_hOutput == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to create console screen buffers.");
        return SCREEN_WIN_BUFFER_CREATION_FAILED;
    }

    Logger::Debug(L"Setting buffer size.");
    SetConsoleScreenBufferSize(_hOutput, dwBufferSize);

    Logger::Debug(L"Setting console window info to minimal size.");
    SMALL_RECT m_rectWindow = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(_hOutput, TRUE, &m_rectWindow);

	Logger::Debug(L"Creating console font.");
    if (IsWindowsTerminal()) {
        screen.SetFontModernTerminal(_hOutput, fontSize);
    }
    else {
        screen.SetFontConsole(_hOutput, fontSize);
    }

    Logger::Debug(L"Setting physical size of console window.");
    SetConsoleWindowInfo(_hOutput, TRUE, &rcRegion);

    Logger::Debug(L"Setting _hOutput as the active console screen buffer.");
    SetConsoleActiveScreenBuffer(_hOutput);

    Logger::Debug(L"Setting console cursor to invisible.");
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(_hOutput, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(_hOutput, &cursorInfo);

    Logger::Debug(L"Disabling window resizing");
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_MAXIMIZEBOX;
        style &= ~WS_SIZEBOX;
        SetWindowLong(hwnd, GWL_STYLE, style);
    } else {
        Logger::Error(L"Failed to get console window handle for disabling resizing.");
    }

    Logger::Debug(L"Deleting old buffers and creating new ones.");
	if (pixelBuffer) { delete[] pixelBuffer; pixelBuffer = nullptr; }
	pixelBuffer = new CHAR_INFO[adjustedWidth * adjustedHeight];

    return SCREEN_NOERROR;
}

void Screen::WindowsImpl::ClearBuffer() {
    std::fill(pixelBuffer, pixelBuffer + screen._true_screen_width * screen._screen_height, CHAR_INFO{'\0', screen._backgroundCol});
}

void Screen::WindowsImpl::OutputBuffer() {
    WriteConsoleOutput(_hOutput, pixelBuffer, dwBufferSize, dwBufferCoord, &rcRegion);
}

void Screen::WindowsImpl::RenderTitle(bool showFps) {
    char titleBuffer[256];

    if (showFps) {
        sprintf_s(
            titleBuffer, sizeof(titleBuffer),
            "[WIN] ASCIIGL - Console Game Engine - %ls - FPS: %.2f",
            screen._title.c_str(), std::min(screen._fps, static_cast<double>(screen._fpsCap))
        );
    } else {
        sprintf_s(
            titleBuffer, sizeof(titleBuffer),
            "[WIN] ASCIIGL - Console Game Engine - %ls",
            screen._title.c_str()
        );
    }

    SetConsoleTitleA(titleBuffer);
}

void Screen::WindowsImpl::PlotPixel(glm::vec2 p, char character, short Colour) {
    int x = static_cast<int>(p.x) * 2; // Double the x coordinate for wide buffer
    int y = static_cast<int>(p.y);
    if (x >= 0 && x < static_cast<int>(screen._true_screen_width) - 1 && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot the pixel twice horizontally
        pixelBuffer[y * screen._true_screen_width + x].Char.AsciiChar = character;
        pixelBuffer[y * screen._true_screen_width + x].Attributes = Colour;
        pixelBuffer[y * screen._true_screen_width + x + 1].Char.AsciiChar = character;
        pixelBuffer[y * screen._true_screen_width + x + 1].Attributes = Colour;
    }
}

void Screen::WindowsImpl::PlotPixel(glm::vec2 p, CHAR_INFO charCol) {
    int x = static_cast<int>(p.x) * 2; // Double the x coordinate for wide buffer
    int y = static_cast<int>(p.y);
    if (x >= 0 && x < static_cast<int>(screen._true_screen_width) - 1 && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot the pixel twice horizontally
        pixelBuffer[y * screen._true_screen_width + x] = charCol;
        pixelBuffer[y * screen._true_screen_width + x + 1] = charCol;
    }
}

void Screen::WindowsImpl::PlotPixel(int x, int y, char character, short Colour) {
    x *= 2; // Double the x coordinate for wide buffer
    if (x >= 0 && x < static_cast<int>(screen._true_screen_width) - 1 && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot the pixel twice horizontally
        pixelBuffer[y * screen._true_screen_width + x].Char.AsciiChar = character;
        pixelBuffer[y * screen._true_screen_width + x].Attributes = Colour;
        pixelBuffer[y * screen._true_screen_width + x + 1].Char.AsciiChar = character;
        pixelBuffer[y * screen._true_screen_width + x + 1].Attributes = Colour;
    }
}

void Screen::WindowsImpl::PlotPixel(int x, int y, CHAR_INFO charCol) {
    x *= 2; // Double the x coordinate for wide buffer
    if (x >= 0 && x < static_cast<int>(screen._true_screen_width) - 1 && y >= 0 && y < static_cast<int>(screen._screen_height)) {
        // Plot the pixel twice horizontally
        pixelBuffer[y * screen._true_screen_width + x] = charCol;
        pixelBuffer[y * screen._true_screen_width + x + 1] = charCol;
    }
}

CHAR_INFO* Screen::WindowsImpl::GetPixelBuffer() {
    return pixelBuffer;
}

bool Screen::WindowsImpl::IsWindowsTerminal() {
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

#else
// std::unique_ptr<Screen::GenericImpl> Screen::genericImpl = std::make_unique<Screen::GenericImpl>();
#endif

// Custom constructor and destructor for PIMPL pattern - must be defined where WindowsImpl is complete
Screen::Screen() = default;
Screen::~Screen() = default;

int Screen::InitializeScreen(
    const unsigned int visible_width, 
    const unsigned int height, 
    const std::wstring title, 
    const unsigned int fontSize, 
    const unsigned int fpsCap, 
    const float fpsWindowSec, 
    const unsigned short backgroundCol
) {
    Logger::Debug(L"CPU has max " + std::to_wstring(std::thread::hardware_concurrency()) + L" threads.");

    Logger::Debug(L"Setting _fpsCap= " + std::to_wstring(fpsCap) + L" and fpsWindow=" + std::to_wstring(fpsWindowSec));
    _fpsCap = fpsCap;
    _fpsWindowSec = fpsWindowSec;

    Logger::Debug(L"Initializing screen with width=" + std::to_wstring(visible_width) + L", height=" + std::to_wstring(height) + L", title=" + title);
    Logger::Debug(L"Requested true width is " + std::to_wstring(visible_width * 2));
    _true_screen_width = visible_width * 2;
    _visible_screen_width = visible_width;
    _screen_height = height;
    _title = title;

    CalculateTileCounts();

    // Enforce minimum font size
    const unsigned int MIN_FONT_SIZE = 2;
    unsigned int adjustedFontSize = fontSize;
    
    if (fontSize < MIN_FONT_SIZE) {
        adjustedFontSize = MIN_FONT_SIZE;
        Logger::Warning(L"Font size " + std::to_wstring(fontSize) + L" is below minimum of " + 
                       std::to_wstring(MIN_FONT_SIZE) + L". Adjusting to minimum.");
    }
    
    Logger::Debug(L"Setting font size to " + std::to_wstring(adjustedFontSize));
    _fontSize = adjustedFontSize;

#ifdef _WIN32
    // Use unified Windows implementation for both CMD and Windows Terminal
    _impl = std::make_unique<WindowsImpl>(*this);
    
    // Windows-specific initialization through delegation
    int initResult = _impl->Initialize(_true_screen_width, height, adjustedFontSize);
    if (initResult != SCREEN_NOERROR) {
        return initResult;
    }
#else
    // Generic initialization for non-Windows platforms
    Logger::Debug(L"Initializing generic console implementation.");
    // TODO: Add generic console initialization here when needed
#endif

    Logger::Debug(L"Deleting old buffers and creating new ones.");
    // Note: Windows pixel buffer is handled in WindowsImpl::Initialize
    
    if (depthBuffer) { delete[] depthBuffer; depthBuffer = nullptr; }
    depthBuffer = new float[_visible_screen_width * height];

    Logger::Debug(L"Clearing buffers for first draw.");
    _backgroundCol = backgroundCol;
    ClearBuffer();

    Logger::Debug(L"Setting console title.");
    RenderTitle(true);

    Logger::Debug(L"Screen initialization complete.");
    return SCREEN_NOERROR;
}

void Screen::RenderTitle(bool showFps) {
    _impl->RenderTitle(showFps);
}

void Screen::ClearBuffer() {
	// clears the buffer by setting the entire buffer to spaces (ascii code 32)
    _impl->ClearBuffer();
	std::fill(depthBuffer, depthBuffer + _visible_screen_width * _screen_height, -1.0f);
}

void Screen::OutputBuffer() {
    _impl->OutputBuffer();
}

void Screen::PlotPixel(glm::vec2 p, char character, short Colour) {
    _impl->PlotPixel(p, character, Colour);
}

void Screen::PlotPixel(glm::vec2 p, CHAR_INFO charCol) {
    _impl->PlotPixel(p, charCol);
}

void Screen::PlotPixel(int x, int y, char character, short Colour) {
    _impl->PlotPixel(x, y, character, Colour);
}

void Screen::PlotPixel(int x, int y, CHAR_INFO charCol) {
    _impl->PlotPixel(x, y, charCol);
}

float Screen::GetDeltaTime() {
	return _deltaTime;
}
    
std::wstring Screen::GetTitle() {
    return _title;
}

void Screen::SetTitle(const std::wstring& title) {
    _title = title;
}

unsigned int Screen::GetFontSize() {
    return _fontSize;
}

unsigned int Screen::GetVisibleWidth() {
    return _visible_screen_width;
}

unsigned int Screen::GetTrueWidth() {
    return _true_screen_width;
}

unsigned int Screen::GetHeight() {
    return _screen_height;
}

unsigned int Screen::GetTileCountX() {
    return TILE_COUNT_X;
}

unsigned int Screen::GetTileCountY() {
    return TILE_COUNT_Y;
}

unsigned int Screen::GetTileSizeX() {
    return TILE_SIZE_X;
}

unsigned int Screen::GetTileSizeY() {
    return TILE_SIZE_Y;
}

void Screen::SetTileSize(const unsigned int x, const unsigned int y) {
    TILE_SIZE_X = x;
    TILE_SIZE_Y = y;
    CalculateTileCounts();
}

void Screen::CalculateTileCounts() {
    // Use ceiling division to ensure all pixels are covered by tiles
    TILE_COUNT_X = (_true_screen_width + TILE_SIZE_X - 1) / TILE_SIZE_X;
    TILE_COUNT_Y = (_screen_height + TILE_SIZE_Y - 1) / TILE_SIZE_Y;
}

unsigned short Screen::GetBackgroundColor() {
    return _backgroundCol;
}

void Screen::SetBackgroundColor(unsigned short color) {
    _backgroundCol = color;
}

CHAR_INFO* Screen::GetPixelBuffer() {
    return _impl->GetPixelBuffer();
}

float* Screen::GetDepthBuffer() {
    return depthBuffer;
}

void Screen::StartFPSClock() {
    StartFPSSample();
}

void Screen::EndFPSClock() {
    EndFPSSample();
    FPSSampleCalculate(GetDeltaTime());
    CapFPS();
}

void Screen::StartFPSSample() {
    startTimeFps = std::chrono::system_clock::now();
}

void Screen::EndFPSSample() {
    endTimeFps = std::chrono::system_clock::now();
    std::chrono::duration<float> deltaTimeTemp = endTimeFps - startTimeFps;
    _deltaTime = deltaTimeTemp.count();
}

void Screen::CapFPS() {
    const float inverseFrameCap = (1.0f / _fpsCap);

    if (_deltaTime < inverseFrameCap) {
        std::this_thread::sleep_for(std::chrono::duration<double>(inverseFrameCap - _deltaTime));
        _deltaTime = inverseFrameCap; // Ensure _deltaTime is at least the frame cap
    }
}

void Screen::FPSSampleCalculate(const double currentDeltaTime) {
    _frameTimes.push_back(currentDeltaTime);
    _currDeltaSum += currentDeltaTime;

    while (!_frameTimes.empty() && _currDeltaSum > _fpsWindowSec) {
        const double popped_front = _frameTimes.front();
        _frameTimes.pop_front();
        _currDeltaSum -= popped_front;
    }

    double calculatedFps = _frameTimes.size() * (1 / _currDeltaSum);
    _fps = calculatedFps;
}

void Screen::Cleanup() {
    _impl.reset();
}

void Screen::SetFontConsole(HANDLE currentHandle, unsigned int fontSize) {
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

void Screen::SetFontModernTerminal(HANDLE currentHandle, unsigned int fontSize) {
    Logger::Info(L"Attempting to modify Windows Terminal settings.json file directly.");
    
    // Convert pixel-based font size to Windows Terminal point size
    float terminalFontSize = _impl->ConvertPixelSizeToTerminalPoints(fontSize);
    Logger::Debug(L"Converting pixel size " + std::to_wstring(fontSize) + 
                  L" to terminal point size " + std::to_wstring(terminalFontSize));
    
    std::wstring settingsPath = _impl->GetWindowsTerminalSettingsPath();
    if (!settingsPath.empty() && _impl->ModifyWindowsTerminalFont(settingsPath, terminalFontSize)) {
        Logger::Info(L"Successfully modified Windows Terminal settings.json");
    }
}

float Screen::WindowsImpl::ConvertPixelSizeToTerminalPoints(unsigned int pixelSize) {
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

std::wstring Screen::WindowsImpl::GetWindowsTerminalSettingsPath() {
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

bool Screen::WindowsImpl::ModifyWindowsTerminalFont(const std::wstring& settingsPath, float fontSize) {             
    try {   
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::string settingsPathStr = converter.to_bytes(settingsPath);
        
        // Read file
        std::wifstream file(settingsPathStr);
        if (!file.is_open()) {
            Logger::Error(L"Could not open Windows Terminal settings file.");
            return false;
        }
        
        std::wstringstream buffer;
        buffer << file.rdbuf();
        file.close();
        std::wstring content = buffer.str();
        
        // Create backup
        std::filesystem::copy_file(settingsPath, settingsPath + L".backup", 
                                 std::filesystem::copy_options::overwrite_existing);
        
        // Simple regex replacements - just update existing values or add minimal font object
        std::wregex sizeRegex(LR"("size"\s*:\s*[\d.]+)");  // Support both integers and floats
        std::wregex faceRegex(LR"("face"\s*:\s*"[^"]*")");
        std::wregex lineHeightRegex(LR"("lineHeight"\s*:\s*[\d.]+)");  // Line height regex
        
        // Update size if it exists
        if (std::regex_search(content, sizeRegex)) {
            content = std::regex_replace(content, sizeRegex, L"\"size\": " + std::to_wstring(fontSize));
        }
        
        // Update line height to minimum (1.0) if it exists
        if (std::regex_search(content, lineHeightRegex)) {
            content = std::regex_replace(content, lineHeightRegex, L"\"lineHeight\": 1.0");
        }
        
        if (!IsFontInstalled(L"Perfect DOS VGA 437")) {
            // Attempt to install font from bundled resources
            std::wstring fontPath = L"res\\ASCIIgL\\fonts\\perfect_dos_vga_437\\Perfect DOS VGA 437.ttf";
            if (InstallFontFromFile(fontPath)) {
                Logger::Info(L"Successfully installed 'Perfect DOS VGA 437' font.");
            } else {
                Logger::Warning(L"Failed to install 'Perfect DOS VGA 437' font. Font face setting may not apply correctly.");
            }
        }

        // Update face if it exists  
        if (std::regex_search(content, faceRegex)) {
            content = std::regex_replace(content, faceRegex, L"\"face\": \"Perfect DOS VGA 437\"");  // True square font at all sizes
        }
        
        // If line height doesn't exist but other font settings do, add it
        if ((std::regex_search(content, sizeRegex) || std::regex_search(content, faceRegex)) && 
            !std::regex_search(content, lineHeightRegex)) {
            // Find font object and add line height
            std::wregex fontObjectRegex(LR"("font"\s*:\s*\{([^}]*)\})");
            std::wsmatch match;
            if (std::regex_search(content, match, fontObjectRegex)) {
                std::wstring fontContent = match[1].str();
                // Add line height to the font object
                std::wstring newFontContent = fontContent + L", \"lineHeight\": 1.0";
                std::wstring replacement = L"\"font\": {" + newFontContent + L"}";
                content = std::regex_replace(content, fontObjectRegex, replacement);
            }
        }
        
        // If no font settings exist, add simple font object to defaults
        if (!std::regex_search(content, sizeRegex) && !std::regex_search(content, faceRegex)) {
            std::wregex defaultsRegex(LR"("defaults"\s*:\s*\{)");
            if (std::regex_search(content, defaultsRegex)) {
                std::wstring fontAdd = L"\"defaults\": {\n        \"font\": {\"size\": " + 
                                      std::to_wstring(fontSize) + L", \"face\": \"Perfect DOS VGA 437\", \"lineHeight\": 1.0},";  // Perfect square font with minimum line height
                content = std::regex_replace(content, defaultsRegex, fontAdd);
            }
        }
        
        // Write back
        std::wofstream outFile(settingsPathStr);
        if (!outFile.is_open()) return false;
        outFile << content;
        outFile.close();
        
        Logger::Debug(L"Modified Windows Terminal settings");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error(L"Failed to modify Windows Terminal settings");
        return false;
    }
}

bool Screen::WindowsImpl::IsFontInstalled(const std::wstring& fontName) {
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

bool Screen::WindowsImpl::InstallFontFromFile(const std::wstring& fontPath) {
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
    bool isInstalled = IsFontInstalled(L"Perfect DOS VGA 437");
    if (isInstalled) {
        Logger::Info(L"Font verification successful: Perfect DOS VGA 437 is now available");
    } else {
        Logger::Warning(L"Font verification failed: Perfect DOS VGA 437 may not be properly registered");
        Logger::Info(L"Suggestion: Restart the application or Windows Terminal, or install the font manually");
    }
    
    return true;
}