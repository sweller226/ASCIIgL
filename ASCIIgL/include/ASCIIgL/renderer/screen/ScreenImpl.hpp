#pragma once

#include <vector>
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

class Screen;

/// Abstract interface for Screen backends (terminal or window).
/// Screen holds a unique_ptr<ScreenImpl> and delegates all buffer/title/output to it.
class ScreenImpl {
public:
    virtual ~ScreenImpl() = default;

    virtual int Initialize(unsigned int width, unsigned int height, float fontSize, const Palette& palette) = 0;
    virtual void ClearPixelBuffer() = 0;
    virtual void OutputBuffer() = 0;
    virtual void RenderTabTitle() = 0;
    virtual void PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour) = 0;
    virtual void PlotPixel(const glm::vec2& p, const CHAR_INFO charCol) = 0;
    virtual void PlotPixel(int x, int y, WCHAR character, unsigned short Colour) = 0;
    virtual void PlotPixel(int x, int y, const CHAR_INFO charCol) = 0;
    virtual void PlotPixel(int idx, const CHAR_INFO charCol) = 0;
    virtual std::vector<CHAR_INFO>& GetPixelBuffer() = 0;
    /// Window handle (console or app window). nullptr if not initialized.
    virtual HWND GetWindowHandle() = 0;
    /// Pump Win32 messages (window mode); no-op in terminal. Sets Screen exit flag on WM_QUIT.
    virtual void ProcessMessages() = 0;
};

} // namespace ASCIIgL
