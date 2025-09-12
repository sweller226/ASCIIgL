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
    std::unique_ptr<WindowsImpl> _impl;
#else
    // std::unique_ptr<GenericImpl> genericImpl;
#endif

    // Platform-agnostic member variables (now instance members)
    unsigned int _true_screen_width = 0;
    unsigned int _visible_screen_width = 0;
    unsigned int _screen_height = 0;
    std::wstring _title = L"";
    unsigned int _fontSize = 0;
    unsigned short _backgroundCol = BG_BLACK;

    // Buffers
    float* depthBuffer = nullptr;

    // Tile properties
    unsigned int TILE_COUNT_X = 0;
    unsigned int TILE_COUNT_Y = 0;
    unsigned int TILE_SIZE_X = 64;
    unsigned int TILE_SIZE_Y = 64;
    
    // FPS and timing (now instance members)
    std::chrono::system_clock::time_point startTimeFps = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point endTimeFps = std::chrono::system_clock::now();
    double _fpsWindowSec = 1.0f;
    double _fps = 0.0f;
    double _currDeltaSum = 0.0f;
    double _deltaTime = 0.0f;
    std::deque<double> _frameTimes = {};
    unsigned int _fpsCap = 60;

    // Singleton constructor/destructor
    Screen(); // Custom constructor defined in .cpp for PIMPL
    ~Screen(); // Custom destructor defined in .cpp for PIMPL
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

public:
    static Screen& GetInstance() {
        static Screen instance;
        return instance;
    }

    // Construction
    int InitializeScreen(
        const unsigned int visible_width, 
        const unsigned int height, 
        const std::wstring title, 
        const unsigned int fontSize, 
        const unsigned int fpsCap, 
        const float fpsWindowSec, 
        const unsigned short backgroundCol
    );

    void SetFontConsole(HANDLE currentHandle, unsigned int fontSize);
    void SetFontModernTerminal(HANDLE currentHandle, unsigned int fontSize);

    // Fps management
    void StartFPSClock();
    void EndFPSClock();
    float GetDeltaTime();

    // Rendering and buffer management
    void RenderTitle(bool showFps);
    void ClearBuffer();
    void OutputBuffer();
    void PlotPixel(glm::vec2 p, char character, short Colour);
    void PlotPixel(glm::vec2 p, CHAR_INFO charCol);
    void PlotPixel(int x, int y, char character, short Colour);
    void PlotPixel(int x, int y, CHAR_INFO charCol);

    std::wstring GetTitle();
    void SetTitle(const std::wstring& title);
    unsigned int GetFontSize();
    unsigned int GetVisibleWidth();
    unsigned int GetTrueWidth();
    unsigned int GetHeight();

    // tile properties
    unsigned int GetTileCountX();
    unsigned int GetTileCountY();
    unsigned int GetTileSizeX();
    unsigned int GetTileSizeY();
    void SetTileSize(const unsigned int x, const unsigned int y);
    void CalculateTileCounts();

    unsigned short GetBackgroundColor();
    void SetBackgroundColor(const unsigned short color);

    // Public buffer access (needed for renderer)
    CHAR_INFO* GetPixelBuffer();
    float* GetDepthBuffer();

    // Cleanup
    void Cleanup();

private:
    // FPS management methods (now instance methods)
    void StartFPSSample();
    void EndFPSSample();
    void CapFPS();
    void FPSSampleCalculate(const double currentDeltaTime);
};