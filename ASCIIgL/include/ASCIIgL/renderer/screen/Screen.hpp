#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <memory>

#include <ASCIIgL/renderer/Palette.hpp>

#include <glm/glm.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ASCIIgL {

class ScreenImpl; // Forward declaration

class Screen {
private:
    friend class ScreenTerminalImpl;
    friend class ScreenWindowImpl;
    std::unique_ptr<ScreenImpl> _impl;
    bool _initialized = false;
    bool _renderToTerminal = true;
    std::atomic<bool> _requestedExit{false};
    unsigned int _screen_width = 0;
    unsigned int _screen_height = 0;
    std::wstring _title;
    float _fontSize = 0.0f;
    std::unique_ptr<Palette> _palette;

    Screen();
    ~Screen();
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

public:
    static Screen& GetInst() {
        static Screen instance;
        return instance;
    }
    int Initialize(unsigned int width, unsigned int height, const std::wstring title, float fontSize, const Palette& palette, bool renderToTerminal = true);
    bool IsInitialized() const;
    bool IsRenderToTerminal() const { return _renderToTerminal; }

    void RenderTabTitle();
    void ClearPixelBuffer();
    void OutputBuffer();
    void PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour);
    void PlotPixel(const glm::vec2& p, const CHAR_INFO charCol);
    void PlotPixel(int x, int y, WCHAR character, unsigned short Colour);
    void PlotPixel(int x, int y, const CHAR_INFO charCol);
    void PlotPixel(int idx, const CHAR_INFO charCol);

    const std::wstring& GetTitle() const;
    void SetTitle(const std::wstring& title);
    float GetFontSize() const;
    unsigned int GetHeight() const;
    unsigned int GetWidth() const;
    Palette& GetPalette();
    /// Returns a pointer to the palette as a MonochromePalette if and only if
    /// the current palette is monochrome. Otherwise returns nullptr.
    MonochromePalette* GetMonochromePalette();
    bool IsMonochromePalette() const;
    std::vector<CHAR_INFO>& GetPixelBuffer();
    /// Window handle (console in terminal mode, app window in window mode). nullptr if not initialized.
    HWND GetWindowHandle() const;

    /// Call once per frame. Pumps Win32 messages (window mode); no-op in terminal mode. Sets exit flag on WM_QUIT or console Ctrl.
    void ProcessMessages();
    /// True after user requested exit (closed window, or console Ctrl+C/close). Poll after ProcessMessages().
    bool ShouldExit() const { return _requestedExit.load(std::memory_order_relaxed); }
    /// Request application exit (e.g. from game logic). Safe to call from any thread.
    void RequestExit() { _requestedExit.store(true, std::memory_order_relaxed); }
};

} // namespace ASCIIgL