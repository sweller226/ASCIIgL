#pragma once

#include <ASCIIgL/renderer/screen/ScreenImpl.hpp>
#include <vector>
#include <string>

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
    void PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour) override;
    void PlotPixel(const glm::vec2& p, const CHAR_INFO charCol) override;
    void PlotPixel(int x, int y, WCHAR character, unsigned short Colour) override;
    void PlotPixel(int x, int y, const CHAR_INFO charCol) override;
    void PlotPixel(int idx, const CHAR_INFO charCol) override;
    std::vector<CHAR_INFO>& GetPixelBuffer() override;
    HWND GetWindowHandle() override;
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
