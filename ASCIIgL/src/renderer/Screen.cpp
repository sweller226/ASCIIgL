#include <ASCIIgL/renderer/Screen.hpp>

#include <thread>
#include <chrono>
#include <algorithm>
#include <deque>
#include <string>

#include <ASCIIgL/engine/Logger.hpp>

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
    public:
        // Console handles and coordinate structures
        HANDLE _hOutput = nullptr;
        COORD dwBufferSize = {0, 0};
        COORD dwBufferCoord = {0, 0};
        SMALL_RECT rcRegion = {0, 0, 0, 0};

        // Buffers for Windows console output
        CHAR_INFO* pixelBuffer = nullptr;

        // Constructor
        WindowsImpl() = default;

        // Destructor
        ~WindowsImpl() {
            if (pixelBuffer) {
                delete[] pixelBuffer;
                pixelBuffer = nullptr;
            }
        }

        // Implementation methods
        int Initialize(const unsigned int width, const unsigned int height, const unsigned int fontSize);
        void ClearBuffer();
        void OutputBuffer();
        void RenderTitle(bool showFps);
        void PlotPixel(glm::vec2 p, char character, short Colour);
        void PlotPixel(glm::vec2 p, CHAR_INFO charCol);
        void PlotPixel(int x, int y, char character, short Colour);
        void PlotPixel(int x, int y, CHAR_INFO charCol);
        CHAR_INFO* GetPixelBuffer();
    };

#else
    // No generic implementation is needed yet, only developing on windows right now
#endif

// Static member definitions for platform-specific implementations
#ifdef _WIN32
    std::unique_ptr<Screen::WindowsImpl> Screen::_impl = nullptr;

// WindowsImpl method implementations (Unified Windows console implementation)
int Screen::WindowsImpl::Initialize(const unsigned int width, const unsigned int height, const unsigned int fontSize) {
    // First, get current console handle to check maximum window size with proper font
    HANDLE currentHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (currentHandle == INVALID_HANDLE_VALUE) {
        Logger::Error(L"Failed to get standard output handle.");
        return SCREEN_WIN_BUFFER_CREATION_FAILED;
    }

    // Set the font FIRST to get accurate maximum window size calculations
    Logger::Debug(L"Setting font for accurate size calculations.");
    CONSOLE_FONT_INFOEX fontForSizing;
    fontForSizing.cbSize = sizeof(fontForSizing);
    fontForSizing.nFont = 0;
    fontForSizing.dwFontSize.X = fontSize;
    fontForSizing.dwFontSize.Y = fontSize;
    fontForSizing.FontFamily = FF_DONTCARE;
    fontForSizing.FontWeight = FW_NORMAL;
    wcscpy_s(fontForSizing.FaceName, LF_FACESIZE, L"Lucida Console");
    
    if (!SetCurrentConsoleFontEx(currentHandle, FALSE, &fontForSizing)) {
        Logger::Error(L"Failed to set font for size calculations.");
    }

    // Small delay to ensure font change takes effect
    Sleep(100);

    // NOW get the console screen buffer info with the correct font applied
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(currentHandle, &csbi);

    Logger::Debug(L"Max console window size: " + std::to_wstring(csbi.dwMaximumWindowSize.X) + L"x" + std::to_wstring(csbi.dwMaximumWindowSize.Y));

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
        Logger::Info(L"Requested width " + std::to_wstring(width) +
                       L" exceeds maximum, adjusting to " + std::to_wstring(adjustedWidth));
    }

    if (dimensionsAdjusted) {
        // Update global screen dimensions
        Screen::SCR_WIDTH = adjustedWidth;
        Screen::SCR_HEIGHT = adjustedHeight;
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
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = fontSize;
    cfi.dwFontSize.Y = fontSize;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
	wcscpy_s(cfi.FaceName, LF_FACESIZE, L"Lucida Console");
    if (!SetCurrentConsoleFontEx(_hOutput, false, &cfi))
        Logger::Error(L"Failed to set font for _hOutput.");

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
    std::fill(pixelBuffer, pixelBuffer + Screen::SCR_WIDTH * Screen::SCR_HEIGHT, CHAR_INFO{' ', Screen::_backgroundCol});
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
            Screen::SCR_TITLE.c_str(), std::min(Screen::_fps, static_cast<double>(Screen::_fpsCap))
        );
    } else {
        sprintf_s(
            titleBuffer, sizeof(titleBuffer),
            "[WIN] ASCIIGL - Console Game Engine - %ls",
            Screen::SCR_TITLE.c_str()
        );
    }

    SetConsoleTitleA(titleBuffer);
}

void Screen::WindowsImpl::PlotPixel(glm::vec2 p, char character, short Colour) {
    int x = static_cast<int>(p.x);
    int y = static_cast<int>(p.y);
    if (x >= 0 && x < static_cast<int>(Screen::SCR_WIDTH) && y >= 0 && y < static_cast<int>(Screen::SCR_HEIGHT)) {
        pixelBuffer[y * Screen::SCR_WIDTH + x].Char.AsciiChar = character;
        pixelBuffer[y * Screen::SCR_WIDTH + x].Attributes = Colour;
    }
}

void Screen::WindowsImpl::PlotPixel(glm::vec2 p, CHAR_INFO charCol) {
    int x = static_cast<int>(p.x);
    int y = static_cast<int>(p.y);
    if (x >= 0 && x < static_cast<int>(Screen::SCR_WIDTH) && y >= 0 && y < static_cast<int>(Screen::SCR_HEIGHT)) {
        pixelBuffer[y * Screen::SCR_WIDTH + x] = charCol;
    }
}

void Screen::WindowsImpl::PlotPixel(int x, int y, char character, short Colour) {
    if (x >= 0 && x < static_cast<int>(Screen::SCR_WIDTH) && y >= 0 && y < static_cast<int>(Screen::SCR_HEIGHT)) {
        pixelBuffer[y * Screen::SCR_WIDTH + x].Char.AsciiChar = character;
        pixelBuffer[y * Screen::SCR_WIDTH + x].Attributes = Colour;
    }
}

void Screen::WindowsImpl::PlotPixel(int x, int y, CHAR_INFO charCol) {
    if (x >= 0 && x < static_cast<int>(Screen::SCR_WIDTH) && y >= 0 && y < static_cast<int>(Screen::SCR_HEIGHT)) {
        pixelBuffer[y * Screen::SCR_WIDTH + x] = charCol;
    }
}

CHAR_INFO* Screen::WindowsImpl::GetPixelBuffer() {
    return pixelBuffer;
}

#else
// std::unique_ptr<Screen::GenericImpl> Screen::genericImpl = std::make_unique<Screen::GenericImpl>();
#endif

int Screen::InitializeScreen(
    const unsigned int width, 
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

    Logger::Debug(L"Initializing screen with width=" + std::to_wstring(width) + L", height=" + std::to_wstring(height) + L", title=" + title);
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    SCR_TITLE = title;

    CalculateTileCounts();

    Logger::Debug(L"Setting font size to " + std::to_wstring(fontSize));
    _fontSize = fontSize;

#ifdef _WIN32
    // Use unified Windows implementation for both CMD and Windows Terminal
    _impl = std::make_unique<WindowsImpl>();
    
    // Windows-specific initialization through delegation
    int initResult = _impl->Initialize(width, height, fontSize);
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
    depthBuffer = new float[width * height];

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
	std::fill(depthBuffer, depthBuffer + SCR_WIDTH * SCR_HEIGHT, -1.0f);
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
    return SCR_TITLE;
}

void Screen::SetTitle(const std::wstring& title) {
    SCR_TITLE = title;
}

unsigned int Screen::GetFontSize() {
    return _fontSize;
}

unsigned int Screen::GetWidth() {
    return SCR_WIDTH;
}

unsigned int Screen::GetHeight() {
    return SCR_HEIGHT;
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
    TILE_COUNT_X = SCR_WIDTH / TILE_SIZE_X;
    TILE_COUNT_Y = SCR_HEIGHT / TILE_SIZE_Y;
}

unsigned short Screen::GetBackgroundColor() {
    return _backgroundCol;
}

void Screen::SetBackgroundColor(unsigned short color) {
    _backgroundCol = color;
}

CHAR_INFO* Screen::GetPixelBuffer() {
#ifdef _WIN32
    return _impl->GetPixelBuffer();
#else
    return nullptr;
#endif
}

float* Screen::GetDepthBuffer() {
    return depthBuffer;
}

void Screen::StartFPSClock() {
    GetInstance().StartFPSSample();
}

void Screen::EndFPSClock() {
    GetInstance().EndFPSSample();
    GetInstance().FPSSampleCalculate(Screen::GetInstance().GetDeltaTime());
    GetInstance().CapFPS();
}

void Screen::StartFPSSample() {
    startTimeFps = std::chrono::system_clock::now();
}

void Screen::EndFPSSample() {
    endTimeFps = std::chrono::system_clock::now();
    std::chrono::duration<float> deltaTimeTemp = endTimeFps - startTimeFps;
    _deltaTime = deltaTimeTemp.count();
    Logger::Debug("Frame _deltaTime: " + std::to_string(_deltaTime));
}

void Screen::CapFPS() {
    Logger::Debug("CapFPS called. _deltaTime: " + std::to_string(_deltaTime) + ", _fpsCap: " + std::to_string(_fpsCap));
    const float inverseFrameCap = (1.0f / _fpsCap);

    if (_deltaTime < inverseFrameCap) {
        Logger::Debug("Sleeping for " + std::to_string(inverseFrameCap - _deltaTime) + " seconds to cap FPS.");
        std::this_thread::sleep_for(std::chrono::duration<double>(inverseFrameCap - _deltaTime));
        _deltaTime = inverseFrameCap; // Ensure _deltaTime is at least the frame cap
    }
}

void Screen::FPSSampleCalculate(const double currentDeltaTime) {
    Logger::Debug("FPSSampleCalculate called. Current _deltaTime: " + std::to_string(currentDeltaTime));
    _frameTimes.push_back(currentDeltaTime);
    _currDeltaSum += currentDeltaTime;
    Logger::Debug("_frameTimes size after push: " + std::to_string(_frameTimes.size()));

    while (!_frameTimes.empty() && _currDeltaSum > _fpsWindowSec) {
        const double popped_front = _frameTimes.front();
        _frameTimes.pop_front();
        _currDeltaSum -= popped_front;
        Logger::Debug("_frameTimes popped. New size: " + std::to_string(_frameTimes.size()));
    }

    double calculatedFps = _frameTimes.size() * (1 / _currDeltaSum);
    Logger::Debug("Calculated FPS: " + std::to_string(calculatedFps));
    _fps = calculatedFps;
}

void Screen::Cleanup() {
    _impl.reset();
}