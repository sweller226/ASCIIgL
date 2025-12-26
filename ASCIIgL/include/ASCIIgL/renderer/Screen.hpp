#pragma once

#include <string>
#include <vector>
#include <memory>

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
#else
    struct CHAR_INFO {};
#endif

namespace ASCIIgL {

#ifdef _WIN32
    class ScreenWinImpl; // Forward declaration
#endif

class Screen {
private:
#ifdef _WIN32
    friend class ScreenWinImpl;
    std::unique_ptr<ScreenWinImpl> _impl;
#endif
    bool _initialized = false;
    unsigned int _screen_width = 0;
    unsigned int _screen_height = 0;
    std::wstring _title;
    unsigned int _fontSize = 0;
    Palette _palette;

    Screen();
    ~Screen();
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

public:
    static Screen& GetInst() {
        static Screen instance;
        return instance;
    }
    int Initialize(unsigned int width, unsigned int height, const std::wstring title, unsigned int fontSize, const Palette palette = Palette());
    bool IsInitialized() const;

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
    unsigned int GetFontSize() const;
    unsigned int GetHeight() const;
    unsigned int GetWidth() const;
    Palette& GetPalette();
    std::vector<CHAR_INFO>& GetPixelBuffer();
};

} // namespace ASCIIgL