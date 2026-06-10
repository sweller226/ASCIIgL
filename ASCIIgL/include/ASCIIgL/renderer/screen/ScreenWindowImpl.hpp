#pragma once

#include <ASCIIgL/renderer/screen/ScreenImpl.hpp>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ASCIIgL {

class Screen;

/// Window output implementation: displays the ASCII buffer in a separate Win32 window.
/// Stub: creates window and buffer; full ASCII rendering to window (e.g. via GPU) can be added later.
class ScreenWindowImpl : public ScreenImpl {
private:
    Screen& screen;
    HWND _hwnd = nullptr;
    std::vector<ScreenPixel> _pixelBuffer;

public:
    ScreenWindowImpl(Screen& screenRef);
    ~ScreenWindowImpl() override;

    int Initialize(unsigned int width, unsigned int height, float fontSize, const Palette& palette) override;
    void ClearPixelBuffer() override;
    void OutputBuffer() override;
    void RenderTabTitle() override;
    void PlotPixel(const glm::vec2& p, wchar_t character, unsigned short Colour) override;
    void PlotPixel(const glm::vec2& p, const ScreenPixel& charCol) override;
    void PlotPixel(int x, int y, wchar_t character, unsigned short Colour) override;
    void PlotPixel(int x, int y, const ScreenPixel& charCol) override;
    void PlotPixel(int idx, const ScreenPixel& charCol) override;
    ScreenPixel* GetPixelBufferData() override;
    const ScreenPixel* GetPixelBufferData() const override;
    size_t GetPixelBufferSize() const override;
    NativeWindowHandle GetWindowHandle() override;
    void ProcessMessages() override;
};

} // namespace ASCIIgL
