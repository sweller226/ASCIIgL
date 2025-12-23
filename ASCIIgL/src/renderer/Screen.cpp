// header include
#include <ASCIIgL/renderer/Screen.hpp>

// c++ standard library
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
#include <iterator>
#include <locale>
#include <codecvt>

// ASCIIgL includes
#include <ASCIIgL/util/Logger.hpp>

#ifdef _WIN32
    #include <ASCIIgL/renderer/ScreenWinImpl.hpp>
#endif

// Custom constructor and destructor for PIMPL pattern - must be defined where ScreenWinImpl is complete
Screen::Screen() = default;
Screen::~Screen() {
    _impl.reset();
}

int Screen::Initialize(
    unsigned int width, 
    unsigned int height, 
    const std::wstring title, 
    unsigned int fontSize, 
    const Palette palette
) {
    if (!_initialized) {
        Logger::Info("Initializing Screen...");
    } else {
        Logger::Warning("Screen is already initialized!");
        return 0;
    }

    Logger::Debug(L"CPU has max " + std::to_wstring(std::thread::hardware_concurrency()) + L" threads.");

    Logger::Debug(L"Initializing screen with width=" + std::to_wstring(width) + L", height=" + std::to_wstring(height) + L", title=" + title);
    Logger::Debug(L"Screen now using square font - single width variable");
    _screen_width = width;  // Single width for square fonts
    _screen_height = height;
    _title = title;

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

    // setting palette reference
    if (palette.entries.size() != Palette::COLOR_COUNT) {
        Logger::Warning(L"Palette does not have exactly " + std::to_wstring(Palette::COLOR_COUNT) + L" colors. Using default palette.");
        _palette = Palette(); // Default palette
    } else {
        _palette = palette;
    }

#ifdef _WIN32
    // Use unified Windows implementation for both CMD and Windows Terminal
    _impl = std::make_unique<ScreenWinImpl>(*this);
    
    // Windows-specific initialization through delegation
    int initResult = _impl->Initialize(width, height, adjustedFontSize, palette);
    if (initResult) { return initResult; }

#else
    // Generic initialization for non-Windows platforms
    Logger::Debug(L"Initializing generic console implementation.");
    // TODO: Add generic console initialization here when needed
#endif

    Logger::Debug(L"Deleting old buffers and creating new ones.");
    // Note: Windows pixel buffer is handled in ScreenWinImpl::Initialize

    Logger::Debug(L"Clearing buffers for first draw.");
    ClearPixelBuffer();

    Logger::Debug(L"Setting console title.");
    RenderTabTitle();

    Logger::Debug(L"Screen initialization complete.");
    
    _initialized = true;
    return 0;
}

void Screen::RenderTabTitle() {
    _impl->RenderTabTitle();
}

void Screen::ClearPixelBuffer() {
	// clears the buffer by setting the entire buffer to spaces (ascii code 32)
    _impl->ClearPixelBuffer();
}

void Screen::OutputBuffer() {
    _impl->OutputBuffer();
}

void Screen::PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour) {
    _impl->PlotPixel(p, character, Colour);
}

void Screen::PlotPixel(const glm::vec2& p, const CHAR_INFO charCol) {
    _impl->PlotPixel(p, charCol);
}

void Screen::PlotPixel(int x, int y, WCHAR character, unsigned short Colour) {
    _impl->PlotPixel(x, y, character, Colour);
}

void Screen::PlotPixel(int x, int y, const CHAR_INFO charCol) {
    _impl->PlotPixel(x, y, charCol);
}

void Screen::PlotPixel(int idx, const CHAR_INFO charCol) {
    _impl->PlotPixel(idx, charCol); // Divide x by 2 for wide buffer
}

const std::wstring& Screen::GetTitle() const {
    return _title;
}

void Screen::SetTitle(const std::wstring& title) {
    _title = title;
}

unsigned int Screen::GetFontSize() const {
    return _fontSize;
}

unsigned int Screen::GetWidth() const {
    return _screen_width;
}

unsigned int Screen::GetHeight() const {
    return _screen_height;
}

std::vector<CHAR_INFO>& Screen::GetPixelBuffer() {
    return _impl->GetPixelBuffer();
}

bool Screen::IsInitialized() const {
    return _initialized;
}

Palette& Screen::GetPalette() {
    return _palette;
}