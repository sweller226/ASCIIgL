#include <ASCIIgL/renderer/screen/ScreenWindowImpl.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/CoverageJson.hpp>

#include <algorithm>
#include <string>
#include <windows.h>

namespace ASCIIgL {

namespace {

const wchar_t* WINDOW_CLASS_NAME = L"ASCIIgL_ScreenWindowImpl";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // Stub: no ASCII drawing yet; just validate
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

bool RegisterWindowClass() {
    static bool registered = false;
    if (registered) return true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        Logger::Error(L"ScreenWindowImpl: Failed to register window class.");
        return false;
    }
    registered = true;
    return true;
}

} // namespace

ScreenWindowImpl::ScreenWindowImpl(Screen& screenRef) : screen(screenRef) {}

ScreenWindowImpl::~ScreenWindowImpl() {
    if (_hwnd) {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
}

int ScreenWindowImpl::Initialize(unsigned int width, unsigned int height, float fontSize, const Palette& palette) {
    (void)palette;

    if (!RegisterWindowClass()) return -1;

    // Pixels per character cell from coverage JSON (matches CharCoverage tool); fallback if file missing
    int cellPixelsX = 8;
    int cellPixelsY = 16;
    if (!CoverageJson::GetCellSizeForFontSize(fontSize, &cellPixelsX, &cellPixelsY)) {
        Logger::Warning(L"ScreenWindowImpl: Coverage JSON not available; using default cell size 8x16.");
    }

    // Get maximum window size: use primary monitor work area (desktop minus taskbar)
    RECT workArea = {};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        Logger::Warning(L"ScreenWindowImpl: Could not get work area, using default max size.");
        workArea.right = 1920;
        workArea.bottom = 1080;
    }
    int maxPixelsX = workArea.right - workArea.left;
    int maxPixelsY = workArea.bottom - workArea.top;
    unsigned int maxCellsX = static_cast<unsigned int>(maxPixelsX / cellPixelsX);
    unsigned int maxCellsY = static_cast<unsigned int>(maxPixelsY / cellPixelsY);

    Logger::Info(L"ScreenWindowImpl: Max window size (cells): " + std::to_wstring(maxCellsX) + L"x" + std::to_wstring(maxCellsY));

    // Clamp requested dimensions to fit on screen (same behavior as ScreenTerminalImpl)
    unsigned int adjustedWidth = width;
    unsigned int adjustedHeight = height;
    bool dimensionsAdjusted = false;

    if (height > maxCellsY) {
        adjustedHeight = maxCellsY;
        dimensionsAdjusted = true;
        Logger::Info(L"Requested height " + std::to_wstring(height) + L" exceeds maximum, adjusting to " + std::to_wstring(adjustedHeight));
    }
    if (width > maxCellsX) {
        adjustedWidth = maxCellsX;
        dimensionsAdjusted = true;
        Logger::Info(L"Requested width " + std::to_wstring(width) + L" exceeds maximum, adjusting to " + std::to_wstring(adjustedWidth));
    }

    if (dimensionsAdjusted) {
        screen._screen_width = adjustedWidth;
        screen._screen_height = adjustedHeight;
        Logger::Info(L"Screen dimensions adjusted to " + std::to_wstring(adjustedWidth) + L"x" + std::to_wstring(adjustedHeight));
    }

    // Window size = logical size × cell size
    int winWidth = static_cast<int>(adjustedWidth) * cellPixelsX;
    int winHeight = static_cast<int>(adjustedHeight) * cellPixelsY;
    DWORD style = WS_OVERLAPPEDWINDOW;

    _hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        screen._title.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winWidth, winHeight,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (!_hwnd) {
        Logger::Error(L"ScreenWindowImpl: Failed to create window.");
        return -1;
    }

    ShowWindow(_hwnd, SW_SHOW);
    Logger::Info(L"ScreenWindowImpl: Window created.");
    return 0;
}

void ScreenWindowImpl::ClearPixelBuffer() {
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
}

void ScreenWindowImpl::OutputBuffer() {
    // Stub: no presentation to window yet (GPU ASCII pass or GDI blit can be added later)
}

void ScreenWindowImpl::RenderTabTitle() {
    if (_hwnd)
        SetWindowTextW(_hwnd, screen._title.c_str());
}

void ScreenWindowImpl::PlotPixel(const glm::vec2& p, WCHAR character, unsigned short Colour) {
    (void)p; (void)character; (void)Colour;
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
}

void ScreenWindowImpl::PlotPixel(const glm::vec2& p, const CHAR_INFO charCol) {
    (void)p; (void)charCol;
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
}

void ScreenWindowImpl::PlotPixel(int x, int y, WCHAR character, unsigned short Colour) {
    (void)x; (void)y; (void)character; (void)Colour;
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
}

void ScreenWindowImpl::PlotPixel(int x, int y, const CHAR_INFO charCol) {
    (void)x; (void)y; (void)charCol;
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
}

void ScreenWindowImpl::PlotPixel(int idx, const CHAR_INFO charCol) {
    (void)idx; (void)charCol;
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
}

std::vector<CHAR_INFO>& ScreenWindowImpl::GetPixelBuffer() {
    static std::vector<CHAR_INFO> empty;
    static bool warned = false;
    if (!warned) { Logger::Warning(L"ScreenWindowImpl: pixel buffer not used in window mode."); warned = true; }
    return empty;
}

HWND ScreenWindowImpl::GetWindowHandle() {
    return _hwnd;
}

void ScreenWindowImpl::ProcessMessages() {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            screen.RequestExit();
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

} // namespace ASCIIgL
