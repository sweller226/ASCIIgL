#pragma once

// External libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// STL
#include <string>
#include <memory>
#include <chrono>
#include <deque>

// Platform-specific includes
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

// Engine includes
#include <ASCIIgL/renderer/RenderEnums.hpp>

// Platform-specific type definitions
#ifdef _WIN32
    // Windows types are already included above
#else
    // Define minimal types for non-Windows platforms
    struct CHAR_INFO {
        union {
            wchar_t UnicodeChar;
            char AsciiChar;
        } Char;
        unsigned short Attributes;
    };
#endif

// Error codes
enum ScreenError {
    SCREEN_NOERROR = 0,
    SCREEN_WIN_BUFFER_CREATION_FAILED = -1,
    SCREEN_WIN_HEIGHT_TOO_BIG = -2,
    SCREEN_WIN_WIDTH_TOO_BIG = -3
};

class Screen {
private:
    // Platform-specific implementation classes
#ifdef _WIN32
    class WindowsImpl;  // Unified Windows implementation for both CMD and Windows Terminal
    friend class WindowsImpl;  // Allow WindowsImpl to access private members
    static std::unique_ptr<WindowsImpl> _impl;
#else
    // static std::unique_ptr<GenericImpl> genericImpl;
#endif

    // Platform-agnostic member variables (inline static)
    inline static unsigned int _true_screen_width = 0;
    inline static unsigned int _visible_screen_width = 0;
    inline static unsigned int _screen_height = 0;
    inline static std::wstring _title = L"";
    inline static unsigned int _fontSize = 0;
    inline static unsigned short _backgroundCol = BG_BLACK;

    // Buffers
    inline static float* depthBuffer = nullptr;
    
    // Tile properties
    inline static unsigned int TILE_COUNT_X = 0;
    inline static unsigned int TILE_COUNT_Y = 0;
    inline static unsigned int TILE_SIZE_X = 64;
    inline static unsigned int TILE_SIZE_Y = 64;
    
    // FPS and timing (platform-agnostic)
    inline static std::chrono::system_clock::time_point startTimeFps = std::chrono::system_clock::now();
    inline static std::chrono::system_clock::time_point endTimeFps = std::chrono::system_clock::now();
    inline static double _fpsWindowSec = 1.0f;
    inline static double _fps = 0.0f;
    inline static double _currDeltaSum = 0.0f;
    inline static double _deltaTime = 0.0f;
    inline static std::deque<double> _frameTimes = {};
    inline static unsigned int _fpsCap = 60;

    // Singleton instance
    Screen() = default;
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

public:
    static Screen& GetInstance() {
        static Screen instance;
        return instance;
    }

    // Construction
    static int InitializeScreen(
        const unsigned int visible_width, 
        const unsigned int height, 
        const std::wstring title, 
        const unsigned int fontSize, 
        const unsigned int fpsCap, 
        const float fpsWindowSec, 
        const unsigned short backgroundCol
    );

    static void SetFontConsole(HANDLE currentHandle, unsigned int fontSize);
    static void SetFontModernTerminal(HANDLE currentHandle, unsigned int fontSize);

    // Fps management
    static void StartFPSClock();
    static void EndFPSClock();
    static float GetDeltaTime();

    // Rendering and buffer management
    static void RenderTitle(bool showFps);
    static void ClearBuffer();
    static void OutputBuffer();
    static void PlotPixel(glm::vec2 p, char character, short Colour);
    static void PlotPixel(glm::vec2 p, CHAR_INFO charCol);
    static void PlotPixel(int x, int y, char character, short Colour);
    static void PlotPixel(int x, int y, CHAR_INFO charCol);

    static std::wstring GetTitle();
    static void SetTitle(const std::wstring& title);
    static unsigned int GetFontSize();
    static unsigned int GetVisibleWidth();
    static unsigned int GetTrueWidth();
    static unsigned int GetHeight();

    // tile properties
    static unsigned int GetTileCountX();
    static unsigned int GetTileCountY();
    static unsigned int GetTileSizeX();
    static unsigned int GetTileSizeY();
    static void SetTileSize(const unsigned int x, const unsigned int y);
    static void CalculateTileCounts();

    static unsigned short GetBackgroundColor();
    static void SetBackgroundColor(const unsigned short color);

    // Public buffer access (needed for renderer)
    static CHAR_INFO* GetPixelBuffer();
    static float* GetDepthBuffer();

    // Cleanup
    static void Cleanup();

private:
    // FPS management methods (platform-agnostic)
    void StartFPSSample();
    void EndFPSSample();
    void CapFPS();
    void FPSSampleCalculate(const double currentDeltaTime);
};