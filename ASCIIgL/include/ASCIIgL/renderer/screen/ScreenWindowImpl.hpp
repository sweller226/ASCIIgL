#pragma once

#include <ASCIIgL/renderer/screen/ScreenImpl.hpp>
#include <vector>

namespace ASCIIgL {

class Screen;

/// Window output implementation: displays the ASCII buffer in a separate Win32 window.
/// Stub: creates window and buffer; full ASCII rendering to window (e.g. via GPU) can be added later.
class ScreenWindowImpl : public ScreenImpl {
private:
    Screen& screen;
    HWND _hwnd = nullptr;

public:
    ScreenWindowImpl(Screen& screenRef);
    ~ScreenWindowImpl() override;

    int Initialize(unsigned int width, unsigned int height, float fontSize, const Palette& palette) override;
    void ClearPixelBuffer() override;
    void OutputBuffer() override;
    void RenderTabTitle() override;
    void PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour) override;
    void PlotPixel(const glm::vec2& p, const CHAR_INFO charCol) override;
    void PlotPixel(int x, int y, WCHAR character, unsigned short Colour) override;
    void PlotPixel(int x, int y, const CHAR_INFO charCol) override;
    void PlotPixel(int idx, const CHAR_INFO charCol) override;
    std::vector<CHAR_INFO>& GetPixelBuffer() override;
    HWND GetWindowHandle() override;
    void ProcessMessages() override;
};

} // namespace ASCIIgL
