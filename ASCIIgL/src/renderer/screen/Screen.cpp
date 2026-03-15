// header include
#include <ASCIIgL/renderer/screen/Screen.hpp>

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

#include <ASCIIgL/renderer/screen/ScreenImpl.hpp>
#include <ASCIIgL/renderer/screen/ScreenTerminalImpl.hpp>
#include <ASCIIgL/renderer/screen/ScreenWindowImpl.hpp>

namespace ASCIIgL {

// Custom constructor and destructor for PIMPL pattern - impl is complete in ScreenTerminalImpl/ScreenWindowImpl headers
Screen::Screen() : _palette(std::make_unique<Palette>()) {}
Screen::~Screen() {
    _impl.reset();
}

int Screen::Initialize(
    unsigned int width, 
    unsigned int height, 
    const std::wstring title, 
    float fontSize, 
    const Palette& palette,
    bool renderToTerminal
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
    const float MIN_FONT_SIZE = 2.0f;
    const float MAX_FONT_SIZE = 11.0f;
    _fontSize = std::clamp(fontSize, MIN_FONT_SIZE, MAX_FONT_SIZE);
    _renderToTerminal = renderToTerminal;

    Logger::Debug(L"Setting font size to " + std::to_wstring(_fontSize));

    // Clone palette so we preserve type (Palette vs MonochromePalette)
    if (palette.entries.size() != Palette::COLOR_COUNT) {
        Logger::Warning(L"Palette does not have exactly " + std::to_wstring(Palette::COLOR_COUNT) + L" colors. Using default palette.");
        _palette = std::make_unique<Palette>();
    } else {
        _palette = palette.clone();
    }

    // Choose backend based on render target
    if (_renderToTerminal) {
        _impl = std::make_unique<ScreenTerminalImpl>(*this);
        Logger::Debug(L"Using terminal output implementation.");
    } else {
        _impl = std::make_unique<ScreenWindowImpl>(*this);
        Logger::Debug(L"Using window output implementation.");
    }

    int initResult = _impl->Initialize(width, height, _fontSize, *_palette);
    if (initResult) { return initResult; }

    Logger::Debug(L"Deleting old buffers and creating new ones.");
    // Note: Pixel buffer is created in impl-specific Initialize (ScreenTerminalImpl or ScreenWindowImpl)

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

float Screen::GetFontSize() const {
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

HWND Screen::GetWindowHandle() const {
    return _impl ? _impl->GetWindowHandle() : nullptr;
}

void Screen::ProcessMessages() {
    if (_impl)
        _impl->ProcessMessages();
}

bool Screen::IsInitialized() const {
    return _initialized;
}

Palette& Screen::GetPalette() {
    return *_palette;
}

MonochromePalette* Screen::GetMonochromePalette() {
    return dynamic_cast<MonochromePalette*>(_palette.get());
}

bool Screen::IsMonochromePalette() const {
    // Detect dynamic type of the owned palette
    return dynamic_cast<MonochromePalette*>(_palette.get()) != nullptr;
}

} // namespace ASCIIgL