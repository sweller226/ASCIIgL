#pragma once

#include <cstddef>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/screen/ScreenTypes.hpp>
#include <glm/glm.hpp>

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
    virtual void PlotPixel(const glm::vec2& p, wchar_t character, unsigned short Colour) = 0;
    virtual void PlotPixel(const glm::vec2& p, const ScreenPixel& charCol) = 0;
    virtual void PlotPixel(int x, int y, wchar_t character, unsigned short Colour) = 0;
    virtual void PlotPixel(int x, int y, const ScreenPixel& charCol) = 0;
    virtual void PlotPixel(int idx, const ScreenPixel& charCol) = 0;
    virtual ScreenPixel* GetPixelBufferData() = 0;
    virtual const ScreenPixel* GetPixelBufferData() const = 0;
    virtual size_t GetPixelBufferSize() const = 0;
    /// Window handle (console or app window). nullptr if not initialized.
    virtual NativeWindowHandle GetWindowHandle() = 0;
    /// Pump Win32 messages (window mode); no-op in terminal. Sets Screen exit flag on WM_QUIT.
    virtual void ProcessMessages() = 0;
};

} // namespace ASCIIgL
