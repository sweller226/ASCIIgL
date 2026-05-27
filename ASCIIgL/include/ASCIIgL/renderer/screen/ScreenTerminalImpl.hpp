#pragma once

#include <ASCIIgL/renderer/screen/ScreenImpl.hpp>
#include <vector>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ASCIIgL {

class Screen; // Forward declaration

class ScreenTerminalImpl : public ScreenImpl {
private:
    Screen& screen;
    std::vector<CHAR_INFO> _pixelBuffer;

public:
    HANDLE _hOutput = nullptr;
    COORD dwBufferSize = {0, 0};
    COORD dwBufferCoord = {0, 0};
    SMALL_RECT rcRegion = {0, 0, 0, 0};

    ScreenTerminalImpl(Screen& screenRef);
    ~ScreenTerminalImpl() override;

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

    // Windows Terminal methods (not part of ScreenImpl)
    void SetFontTerminal(HANDLE currentHandle, float fontSize);
    std::wstring GetTerminalSettingsPath();
    bool ModifyTerminalFont(const std::wstring& settingsPath, float fontSize);
    void EnableVTMode();
    bool IsFontInstalled(const std::wstring& fontName);
    bool InstallFontFromFile(const std::wstring& fontFilePath);
    void SetPaletteTerminal(const Palette& palette, HANDLE& hOutput);
};

} // namespace ASCIIgL
