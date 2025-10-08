#pragma once

#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/util/Clock.hpp>

// External libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// STL
#include <string>
#include <memory>
#include <chrono>
#include <vector>

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

// Platform-specific type definitions
#ifdef _WIN32
    // Windows types are already included above
#else

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
    bool _initialized = false;

    // Platform-agnostic member variables (now instance members)
    unsigned int _screen_width = 0;  // Single width for square fonts
    unsigned int _screen_height = 0;
    std::wstring _title = L"";
    unsigned int _fontSize = 0;

    // Color palette that the terminal uses
    Palette _palette;

    // Singleton constructor/destructor
    Screen(); // Custom constructor defined in .cpp for PIMPL
    ~Screen(); // Custom destructor defined in .cpp for PIMPL
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

public:
    static Screen& GetInst() {
        static Screen instance;
        return instance;
    }

    // Construction
    int Initialize(
        const unsigned int width, 
        const unsigned int height, 
        const std::wstring title, 
        const unsigned int fontSize, 
        const Palette palette = Palette()
    );
    bool IsInitialized() const;

    // Rendering and buffer management
    void RenderTabTitle();
    void ClearPixelBuffer();
    void OutputBuffer();
    void PlotPixel(const glm::vec2& p, const WCHAR character, const unsigned short Colour);
    void PlotPixel(const glm::vec2& p, const CHAR_INFO charCol);
    void PlotPixel(int x, int y, const WCHAR character, const unsigned short Colour);
    void PlotPixel(int x, int y, const CHAR_INFO charCol);
    void PlotPixel(int idx, const CHAR_INFO charCol); // Overload for 1D index

    // title font and screen properties
    std::wstring GetTitle();
    void SetTitle(const std::wstring& title);
    unsigned int GetFontSize();
    unsigned int GetHeight();
    unsigned int GetWidth();

    // palette
    Palette& GetPalette();

    // Public buffer access (needed for renderer)
    std::vector<CHAR_INFO>& GetPixelBuffer();

};