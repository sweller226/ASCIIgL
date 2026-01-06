#pragma once

#include <vector>
#include <string>
#include <ASCIIgL/renderer/Palette.hpp>
#include <glm/glm.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

namespace ASCIIgL {

class Screen; // Forward declaration

#ifdef _WIN32

class ScreenWinImpl {
private:
    Screen& screen;
    std::vector<CHAR_INFO> _pixelBuffer;

public:
    HANDLE _hOutput = nullptr;
    COORD dwBufferSize = {0, 0};
    COORD dwBufferCoord = {0, 0};
    SMALL_RECT rcRegion = {0, 0, 0, 0};

    ScreenWinImpl(Screen& screenRef);
    ~ScreenWinImpl();

    int Initialize(unsigned int width, unsigned int height, unsigned int fontSize, const Palette& palette);
    void ClearPixelBuffer();
    void OutputBuffer();
    void RenderTabTitle();
    void PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour);
    void PlotPixel(const glm::vec2& p, const CHAR_INFO charCol);
    void PlotPixel(int x, int y, WCHAR character, unsigned short Colour);
    void PlotPixel(int x, int y, const CHAR_INFO charCol);
    void PlotPixel(int idx, const CHAR_INFO charCol);
    std::vector<CHAR_INFO>& GetPixelBuffer();

    // Windows Terminal methods
    void SetFontTerminal(HANDLE currentHandle, unsigned int fontSize);
    std::wstring GetTerminalSettingsPath();
    bool ModifyTerminalFont(const std::wstring& settingsPath, float fontSize);
    float ConvertPixelSizeToTerminalPoints(unsigned int pixelSize);
    void EnableVTMode();
    bool IsFontInstalled(const std::wstring& fontName);
    bool InstallFontFromFile(const std::wstring& fontFilePath);
    void SetPaletteTerminal(const Palette& palette, HANDLE& hOutput);
};

#endif

} // namespace ASCIIgL