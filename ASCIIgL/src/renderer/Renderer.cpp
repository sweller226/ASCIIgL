#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>
#include <execution>
#include <numeric>
#include <sstream>
#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <Windows.h>

#include <nlohmann/json.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>
#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/renderer/Material.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

// Core lifecycle and non-draw functionality for Renderer lives here.
// Draw-call related methods have been moved to Renderer_Draw.cpp.

// =============================================================================
// TRIANGLE RASTERIZATION - WIREFRAME (PIXEL BUFFER)
// =============================================================================

void Renderer::DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const WCHAR pixel_type, const unsigned short col) {
    // RENDERING LINES BETWEEN VERTICES
    DrawLinePxBuff((int) vert1.x, (int) vert1.y, (int) vert2.x, (int) vert2.y, pixel_type, col);
    DrawLinePxBuff((int) vert2.x, (int) vert2.y, (int) vert3.x, (int) vert3.y, pixel_type, col);
    DrawLinePxBuff((int) vert3.x, (int) vert3.y, (int) vert1.x, (int) vert1.y, pixel_type, col);
}

// =============================================================================
// TRIANGLE RASTERIZATION - WIREFRAME (COLOR BUFFER)
// =============================================================================

void Renderer::DrawTriangleWireframeColBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::ivec4& col) {
    // RENDERING LINES BETWEEN VERTICES
    DrawLineColBuff((int) vert1.x, (int) vert1.y, (int) vert2.x, (int) vert2.y, col);
    DrawLineColBuff((int) vert2.x, (int) vert2.y, (int) vert3.x, (int) vert3.y, col);
    DrawLineColBuff((int) vert3.x, (int) vert3.y, (int) vert1.x, (int) vert1.y, col);
}

void Renderer::DrawTriangleWireframeColBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::ivec3& col) {
    DrawTriangleWireframeColBuff(vert1, vert2, vert3, glm::ivec4(col, 1));
}

// =============================================================================
// LINE DRAWING - PIXEL BUFFER
// =============================================================================

void Renderer::DrawLinePxBuff(const int x1, const int y1, const int x2, const int y2, const WCHAR pixel_type, const unsigned short col) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int incx = (x2 > x1) ? 1 : -1;
    int incy = (y2 > y1) ? 1 : -1;
    int x = x1, y = y1;

    Screen::GetInst().PlotPixel(x, y, pixel_type, col);

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            Screen::GetInst().PlotPixel(x, y, pixel_type, col);
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            Screen::GetInst().PlotPixel(x, y, pixel_type, col);
        }
    }
}

void Renderer::DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col) {
    // Bresenham's line algorithm with tile bounds clipping
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int incx = (x1 > x0) ? 1 : -1;
    int incy = (y1 > y0) ? 1 : -1;
    int x = x0, y = y0;

    // Plot first pixel if within bounds
    if (x >= minX && x < maxX && y >= minY && y < maxY) {
        Screen::GetInst().PlotPixel(x, y, pixel_type, col);
    }

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                Screen::GetInst().PlotPixel(x, y, pixel_type, col);
            }
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                Screen::GetInst().PlotPixel(x, y, pixel_type, col);
            }
        }
    }
}

void Renderer::DrawScreenBorderPxBuff(const unsigned short col) {
    wchar_t borderChar = _charRamp.empty() ? L'#' : _charRamp.back();
    DrawLinePxBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, borderChar, col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, borderChar, col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, borderChar, col);
    DrawLinePxBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, borderChar, col);
}

// =============================================================================
// LINE DRAWING - COLOR BUFFER
// =============================================================================

void Renderer::DrawLineColBuff(const int x1, const int y1, const int x2, const int y2, const glm::ivec4& col) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int incx = (x2 > x1) ? 1 : -1;
    int incy = (y2 > y1) ? 1 : -1;
    int x = x1, y = y1;

    PlotColorBlend(x, y, col);

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            PlotColorBlend(x, y, col);
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            PlotColorBlend(x, y, col);
        }
    }
}

void Renderer::DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::ivec3& col) {
    DrawLineColBuff(x1, y1, x2, y2, glm::ivec4(col, 1));
}

void Renderer::DrawClippedLineColBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, const glm::ivec4& col) {
    // Bresenham's line algorithm with tile bounds clipping
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int incx = (x1 > x0) ? 1 : -1;
    int incy = (y1 > y0) ? 1 : -1;
    int x = x0, y = y0;

    // Plot first pixel if within bounds
    if (x >= minX && x < maxX && y >= minY && y < maxY) {
        PlotColorBlend(x, y, col);
    }

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                PlotColorBlend(x, y, col);
            }
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                PlotColorBlend(x, y, col);
            }
        }
    }
}

void Renderer::DrawScreenBorderColBuff(const glm::vec3& col) {
    // DRAWING BORDERS
    DrawLineColBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, col);
    DrawLineColBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, col);
    DrawLineColBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, col);
    DrawLineColBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, col);
}

// =============================================================================
// PIXEL PLOTTING - COLOR BUFFER
// =============================================================================

void Renderer::PlotColor(int x, int y, const glm::ivec3& color) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    _color_buffer[y * screenWidth + x] = glm::ivec4(color, 1);
}

void Renderer::PlotColor(int x, int y, const glm::ivec4& color) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    _color_buffer[y * screenWidth + x] = color;
}

void Renderer::PlotColorBlend(int x, int y, const glm::ivec4& color) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    _color_buffer[y * screenWidth + x] = color;
}

// =============================================================================
// BUFFER MANAGEMENT
// =============================================================================

void Renderer::BeginColBuffFrame() {
    if (!_initialized) {
        Logger::Error("[Renderer] BeginFrame called before initialization!");
        return;
    }

    Logger::Debug("BeginFrame: Clearing render target and setting up pipeline");

    // Clear render target: background is sRGB 0-255; RTV is sRGB so clear expects linear 0-1
    glm::vec3 linearBg = PaletteUtil::sRGB255ToLinear1(GetBackgroundCol());
    float clear_color[4] = { linearBg.r, linearBg.g, linearBg.b, 1.0f };
    _context->ClearRenderTargetView(_renderTargetView.Get(), clear_color);

    // Clear depth stencil
    _context->ClearDepthStencilView(_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);  // Clear to 1.0 (far plane)

    // Bind render targets
    _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilView.Get());

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(Screen::GetInst().GetWidth());
    viewport.Height = static_cast<float>(Screen::GetInst().GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    _context->RSSetViewports(1, &viewport);

    // Set depth stencil and blend state (on for both 3D and 2D)
    _context->OMSetDepthStencilState(_depthStencilState.Get(), 0);
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    _context->OMSetBlendState(_blendStateAlpha.Get(), blendFactor, 0xFFFFFFFF);

    // Set rasterizer state based on wireframe, backface culling, and CCW modes
    bool wireframe = GetWireframe();
    bool backfaceCulling = GetBackfaceCulling();
    bool ccw = GetCCW();
    
    // Calculate index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    int stateIndex = (wireframe ? 1 : 0) + (backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
    _context->RSSetState(_rasterizerStates[stateIndex].Get());

    // Bind sampler
    _context->PSSetSamplers(0, 1, _samplerLinear.GetAddressOf());

    // Set primitive topology
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    std::fill(_color_buffer.begin(), _color_buffer.end(), glm::ivec4(_background_col, 1));
}

std::vector<glm::ivec4>& Renderer::GetColorBuffer() {
    return _color_buffer;
}

void Renderer::OverwritePxBuffWithColBuff() {
    LogDiagnostics();
    ResetDiagnostics();

    auto& screen = Screen::GetInst();
    auto& pixelBuffer = screen.GetPixelBuffer();
    const size_t bufferSize = _color_buffer.size();

    if (_colorLUTState == ColorLUTState::NotComputed) {
        PrecomputeColorLUT();
    }

    if (_colorLUTState == ColorLUTState::Monochrome) {
        for (size_t i = 0; i < bufferSize; ++i) {
            const auto& c = _color_buffer[i];
            float L = PaletteUtil::sRGB255_Luminance(glm::ivec3(c.r, c.g, c.b));
            size_t idx = MonochromeLuminanceToIndex(L);
            pixelBuffer[i] = _monochromeLUT[idx].second;
        }
        return;
    }

    const size_t unrollFactor = 4;
    const size_t unrolledEnd = (bufferSize / unrollFactor) * unrollFactor;
    auto to16 = [](int v) { return (v * 15 + 127) / 255; };
    size_t i = 0;
    for (; i < unrolledEnd; i += unrollFactor) {
        const auto& color0 = _color_buffer[i];
        const auto& color1 = _color_buffer[i + 1];
        const auto& color2 = _color_buffer[i + 2];
        const auto& color3 = _color_buffer[i + 3];
        const int r0 = to16(color0.r), g0 = to16(color0.g), b0 = to16(color0.b);
        const int r1 = to16(color1.r), g1 = to16(color1.g), b1 = to16(color1.b);
        const int r2 = to16(color2.r), g2 = to16(color2.g), b2 = to16(color2.b);
        const int r3 = to16(color3.r), g3 = to16(color3.g), b3 = to16(color3.b);
        const int index0 = (r0 * _rgbLUTDepth * _rgbLUTDepth) + (g0 * _rgbLUTDepth) + b0;
        const int index1 = (r1 * _rgbLUTDepth * _rgbLUTDepth) + (g1 * _rgbLUTDepth) + b1;
        const int index2 = (r2 * _rgbLUTDepth * _rgbLUTDepth) + (g2 * _rgbLUTDepth) + b2;
        const int index3 = (r3 * _rgbLUTDepth * _rgbLUTDepth) + (g3 * _rgbLUTDepth) + b3;
        pixelBuffer[i] = _colorLUT[index0];
        pixelBuffer[i + 1] = _colorLUT[index1];
        pixelBuffer[i + 2] = _colorLUT[index2];
        pixelBuffer[i + 3] = _colorLUT[index3];
    }
    for (; i < bufferSize; ++i) {
        const auto& color = _color_buffer[i];
        const int r = to16(color.r), g = to16(color.g), b = to16(color.b);
        const int index = (r * _rgbLUTDepth * _rgbLUTDepth) + (g * _rgbLUTDepth) + b;
        pixelBuffer[i] = _colorLUT[index];
    }
}

// =============================================================================
// CHAR COVERAGE (loaded from JSON to match char_coverage tool)
// =============================================================================

// Character ramp order must match tools/CharCoverage DEFAULT_CHARS (coverages array index = ramp index).
static const wchar_t CHAR_RAMP_DEFAULT[] =
    L" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890.:,;'\"(!?)+-*/=\"";

void Renderer::LoadCharCoverageFromJson() {
    _charRamp.clear();
    _charCoverage.clear();
    _colorLUTState = ColorLUTState::NotComputed;

    float fontSize = Screen::GetInst().GetFontSize();

    // Try exe-relative (ASCIICraft copy) then project-root-relative (run from ASCIICraft root)
    const char* paths[] = {
        "res/ASCIIgL/coverage_cleartype.json",
    };
    std::ifstream file;
    for (const char* path : paths) {
        file.open(path);
        if (file.good()) break;
        file.close();
    }
    if (!file.good()) {
        Logger::Warning("[Renderer] coverage_cleartype.json not found; using fallback char ramp.");
        for (size_t i = 0; CHAR_RAMP_DEFAULT[i]; ++i) {
            _charRamp.push_back(CHAR_RAMP_DEFAULT[i]);
            _charCoverage.push_back(0.5f);
        }
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(file);
    } catch (const std::exception& e) {
        Logger::Warning(std::string("[Renderer] Failed to parse coverage_cleartype.json: ") + e.what());
        for (size_t i = 0; CHAR_RAMP_DEFAULT[i]; ++i) {
            _charRamp.push_back(CHAR_RAMP_DEFAULT[i]);
            _charCoverage.push_back(0.5f);
        }
        return;
    }
    file.close();

    if (!j.contains("intervals") || !j["intervals"].is_array() || j["intervals"].empty()) {
        Logger::Warning("[Renderer] coverage_cleartype.json has no intervals.");
        for (size_t i = 0; CHAR_RAMP_DEFAULT[i]; ++i) {
            _charRamp.push_back(CHAR_RAMP_DEFAULT[i]);
            _charCoverage.push_back(0.5f);
        }
        return;
    }

    const auto& intervals = j["intervals"];
    size_t bestIdx = 0;
    bool found = false;
    for (size_t i = 0; i < intervals.size(); ++i) {
        const auto& iv = intervals[i];
        if (!iv.contains("sizeMin") || !iv.contains("sizeMax") || !iv.contains("coverages")) continue;
        float smin = iv["sizeMin"].get<float>();
        float smax = iv["sizeMax"].get<float>();
        if (fontSize >= smin && fontSize <= smax) {
            bestIdx = i;
            found = true;
            break;
        }
    }
    if (!found && !intervals.empty()) {
        if (fontSize <= intervals[0]["sizeMin"].get<float>())
            bestIdx = 0;
        else
            bestIdx = intervals.size() - 1;
    }

    const auto& iv = intervals[bestIdx];
    const auto& coverages = iv["coverages"];
    if (!coverages.is_array()) {
        Logger::Warning("[Renderer] Selected interval has no coverages array.");
        for (size_t i = 0; CHAR_RAMP_DEFAULT[i]; ++i) {
            _charRamp.push_back(CHAR_RAMP_DEFAULT[i]);
            _charCoverage.push_back(0.5f);
        }
        return;
    }

    size_t n = coverages.size();
    _charCoverage.reserve(n);
    for (size_t i = 0; i < n; ++i)
        _charCoverage.push_back(coverages[i].get<float>());

    _charRamp.clear();
    _charRamp.reserve(n);
    if (j.contains("chars") && j["chars"].is_array() && j["chars"].size() >= n) {
        for (size_t i = 0; i < n; ++i)
            _charRamp.push_back(static_cast<wchar_t>(j["chars"][i].get<unsigned>()));
    } else {
        for (const wchar_t* p = CHAR_RAMP_DEFAULT; *p && _charRamp.size() < n; ++p)
            _charRamp.push_back(*p);
        while (_charRamp.size() < n)
            _charRamp.push_back(L' ');
    }

    // Sort ramp by coverage ascending (low-coverage chars first, high last)
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), size_t(0));
    std::sort(order.begin(), order.end(), [this](size_t a, size_t b) { return _charCoverage[a] < _charCoverage[b]; });
    std::vector<wchar_t> sortedRamp(n);
    std::vector<float> sortedCoverage(n);
    for (size_t i = 0; i < n; ++i) {
        sortedRamp[i] = _charRamp[order[i]];
        sortedCoverage[i] = _charCoverage[order[i]];
    }
    _charRamp = std::move(sortedRamp);
    _charCoverage = std::move(sortedCoverage);

    Logger::Info("[Renderer] Loaded char coverage for font size " + std::to_string(fontSize) + " (" + std::to_string(_charRamp.size()) + " glyphs)");
    for (size_t i = 0; i < _charRamp.size() && i < _charCoverage.size(); ++i) {
        wchar_t ch = _charRamp[i];
        float cov = _charCoverage[i];
        std::wstring charDesc;
        if (ch == L' ') charDesc = L"' '";
        else if (ch == L'"') charDesc = L"'\"'";
        else if (ch == L'\\') charDesc = L"'\\\\'";
        else if (ch >= 32 && ch < 127) charDesc = std::wstring(L"'") + ch + L"'";
        else {
            charDesc = L"U+";
            wchar_t hex[8];
            swprintf_s(hex, L"%04X", (unsigned)(unsigned short)ch);
            charDesc += hex;
        }
        Logger::Debug(L"[Renderer] char coverage " + charDesc + L" => " + std::to_wstring(cov));
    }
}

// =============================================================================
// COLOR LOOKUP TABLE (LUT)
// =============================================================================

size_t Renderer::MonochromeLuminanceToIndex(float L) const {
    // Monochrome luminance samples were built by linearly interpolating between
    // lowLinear and highLinear and then taking LinearRGB_Luminance, so the
    // luminance samples are effectively uniform between [L_min, L_max].
    // We can therefore invert using a simple affine mapping + round,
    // clamped to [0, _monochromeLUTSize-1].
    const float Lmin = _monochromeLUT.front().first;
    const float Lmax = _monochromeLUT.back().first;
    if (L <= Lmin) return 0;
    if (L >= Lmax) return _monochromeLUTSize - 1;

    const float t = (L - Lmin) / (Lmax - Lmin); // in (0,1)
    float fIdx = t * static_cast<float>(_monochromeLUTSize - 1);
    // Round to nearest index
    size_t idx = static_cast<size_t>(std::floor(fIdx + 0.5f));
    if (idx >= _monochromeLUTSize) idx = _monochromeLUTSize - 1;
    return idx;
}

void Renderer::PrecomputeColorLUT() {
    Palette& palette = Screen::GetInst().GetPalette();
    if (_colorLUTState != ColorLUTState::NotComputed) return;
    if (_charRamp.empty() || _charRamp.size() != _charCoverage.size()) return;

    if (Screen::GetInst().IsMonochromePalette()) {
        PrecomputeMonochromeColorLUT(palette);
        return;
    }

    PrecomputeMultiColorLUT(palette);
}

void Renderer::PrecomputeMonochromeColorLUT(Palette& palette) {
    Logger::Info("[Renderer] Precomputing monochrome color LUT...");

    _colorLUTState = ColorLUTState::Monochrome;

    // ===== MONOCHROME: 1-D LUT sweep from lowest to highest palette color (0..1023) =====
    unsigned int minLumIdx = palette.GetMinLumIdx();
    unsigned int maxLumIdx = palette.GetMaxLumIdx();
    glm::vec3 lowRGB = palette.GetRGBNormalized(minLumIdx);
    glm::vec3 highRGB = palette.GetRGBNormalized(maxLumIdx);
    glm::vec3 lowLinear = PaletteUtil::sRGB1ToLinear1(lowRGB);
    glm::vec3 highLinear = PaletteUtil::sRGB1ToLinear1(highRGB);

    for (int i = 0; i <= 1023; ++i) {
        float t = static_cast<float>(i) / 1023.0f;
        glm::vec3 targetLinear = glm::mix(lowLinear, highLinear, t);
        float targetLuminance = PaletteUtil::LinearRGB_Luminance(targetLinear);

        float minError = FLT_MAX;
        int bestFgIndex = 0, bestBgIndex = 0, bestCharIndex = 0;

        for (int fgIdx = 0; fgIdx < static_cast<int>(palette.COLOR_COUNT); ++fgIdx) {
            float fgLuminance = palette.GetLuminance(fgIdx);
            for (int bgIdx = 0; bgIdx < static_cast<int>(palette.COLOR_COUNT); ++bgIdx) {
                float bgLuminance = palette.GetLuminance(bgIdx);
                for (int charIdx = 0; charIdx < static_cast<int>(_charRamp.size()); ++charIdx) {
                    float coverage = _charCoverage[charIdx];
                    float simLum = coverage * fgLuminance + (1.0f - coverage) * bgLuminance;
                    float error = std::abs(targetLuminance - simLum);
                    if (error < minError) {
                        minError = error;
                        bestFgIndex = fgIdx;
                        bestBgIndex = bgIdx;
                        bestCharIndex = charIdx;
                    }
                }
            }
        }

        wchar_t glyph = _charRamp[bestCharIndex];
        unsigned short fgColor = static_cast<unsigned short>(palette.GetFgColor(bestFgIndex));
        unsigned short bgColor = static_cast<unsigned short>(palette.GetBgColor(bestBgIndex));
        unsigned short combinedColor = fgColor | bgColor;
        if (fgColor > 0xF || bgColor > 0xF0) {
            fgColor = fgColor & 0xF;
            bgColor = bgColor & 0xF0;
            combinedColor = fgColor | bgColor;
        }
        _monochromeLUT[i] = std::make_pair(targetLuminance, CHAR_INFO{ glyph, combinedColor });
    }

    LogMonochromeLUTStats(palette);
}

void Renderer::LogMonochromeLUTStats(Palette& palette) {
    using ASCIIgL::Logger;

    // 1) Count unique CHAR_INFO entries (glyph + color attributes)
    std::unordered_set<uint64_t> uniqueEntries;
    uniqueEntries.reserve(_monochromeLUTSize);

    // 4) Color usage distribution (by palette index)
    const int colorCount = static_cast<int>(palette.COLOR_COUNT);
    std::vector<int> fgCounts(colorCount, 0);
    std::vector<int> bgCounts(colorCount, 0);

    auto attrToFgIndex = [&palette, colorCount](unsigned short fgAttr) -> int {
        for (int i = 0; i < colorCount; ++i) {
            if (palette.GetFgColor(i) == fgAttr)
                return i;
        }
        return -1;
    };
    auto attrToBgIndex = [&palette, colorCount](unsigned short bgAttr) -> int {
        for (int i = 0; i < colorCount; ++i) {
            if (palette.GetBgColor(i) == bgAttr)
                return i;
        }
        return -1;
    };

    // 5) Distinct (fg, bg) pairs (by palette index)
    std::unordered_set<uint32_t> pairSet;
    pairSet.reserve(_monochromeLUTSize);

    int minFgIdx = colorCount, maxFgIdx = -1;
    int minBgIdx = colorCount, maxBgIdx = -1;

    for (size_t i = 0; i < _monochromeLUTSize; ++i) {
        const CHAR_INFO& ci = _monochromeLUT[i].second;
        wchar_t glyph = ci.Char.UnicodeChar;
        unsigned short attrs = ci.Attributes;

        uint64_t key = (static_cast<uint64_t>(glyph) << 16) | attrs;
        uniqueEntries.insert(key);

        unsigned short fgAttr = attrs & 0x0F;
        unsigned short bgAttr = attrs & 0xF0;

        int fgIdx = attrToFgIndex(fgAttr);
        int bgIdx = attrToBgIndex(bgAttr);

        if (fgIdx >= 0) {
            fgCounts[fgIdx]++;
            if (fgIdx < minFgIdx) minFgIdx = fgIdx;
            if (fgIdx > maxFgIdx) maxFgIdx = fgIdx;
        }
        if (bgIdx >= 0) {
            bgCounts[bgIdx]++;
            if (bgIdx < minBgIdx) minBgIdx = bgIdx;
            if (bgIdx > maxBgIdx) maxBgIdx = bgIdx;
        }
        if (fgIdx >= 0 && bgIdx >= 0) {
            uint32_t pairKey = (static_cast<uint32_t>(bgIdx) << 16) | static_cast<uint32_t>(fgIdx & 0xFFFF);
            pairSet.insert(pairKey);
        }
    }

    const size_t totalEntries = _monochromeLUTSize;
    const size_t uniqueCount = uniqueEntries.size();

    int fgUsed = 0, bgUsed = 0;
    for (int i = 0; i < colorCount; ++i) {
        if (fgCounts[i] > 0) fgUsed++;
        if (bgCounts[i] > 0) bgUsed++;
    }

    Logger::Info(
        "[Renderer] Monochrome LUT: " +
        std::to_string(totalEntries) + " entries, " +
        std::to_string(uniqueCount) + " unique CHAR_INFO (" +
        std::to_string((100.0 * uniqueCount) / totalEntries) + "%)"
    );

    Logger::Info(
        "[Renderer] Monochrome LUT: FG colors used " +
        std::to_string(fgUsed) + "/" + std::to_string(colorCount) +
        ", BG colors used " +
        std::to_string(bgUsed) + "/" + std::to_string(colorCount)
    );

    Logger::Info(
        "[Renderer] Monochrome LUT: distinct (fg,bg) pairs = " +
        std::to_string(pairSet.size())
    );

    if (minFgIdx <= maxFgIdx) {
        Logger::Info(
            "[Renderer] Monochrome LUT: FG palette index range [" +
            std::to_string(minFgIdx) + ", " + std::to_string(maxFgIdx) + "]"
        );
    }
    if (minBgIdx <= maxBgIdx) {
        Logger::Info(
            "[Renderer] Monochrome LUT: BG palette index range [" +
            std::to_string(minBgIdx) + ", " + std::to_string(maxBgIdx) + "]"
        );
    }

    // Optional: dump full LUT mapping when diagnostics are enabled
    if (_diagnostics_enabled) {
        Logger::Debug("[Renderer] Monochrome LUT entries (index, targetL, glyphCode, fgIdx, bgIdx, attrs):");
        for (size_t i = 0; i < _monochromeLUTSize; ++i) {
            float targetL = _monochromeLUT[i].first;
            const CHAR_INFO& ci = _monochromeLUT[i].second;
            unsigned short attrs = ci.Attributes;
            unsigned short fgAttr = attrs & 0x0F;
            unsigned short bgAttr = attrs & 0xF0;

            int fgIdx = attrToFgIndex(fgAttr);
            int bgIdx = attrToBgIndex(bgAttr);

            unsigned int glyphCode = static_cast<unsigned int>(ci.Char.UnicodeChar);

            Logger::Debug(
                "[Renderer] MonoLUT[" + std::to_string(i) +
                "]: L=" + std::to_string(targetL) +
                " glyph=U+" + std::to_string(glyphCode) +
                " fgIdx=" + std::to_string(fgIdx) +
                " bgIdx=" + std::to_string(bgIdx) +
                " attrs=" + std::to_string(attrs)
            );
        }
    }
}

void Renderer::PrecomputeMultiColorLUT(Palette& palette) {
    Logger::Info("[Renderer] Precomputing multi-color color LUT...");

    _colorLUTState = ColorLUTState::MultiColor;
    // ===== MULTICOLOR: full 16×16×16 RGB cube =====
    const float invPaletteDepth = 1.0f / 15.0f;
    auto srgbToLinear = [](float s) {
        return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
    };
    auto srgbToLinearVec = [&srgbToLinear](const glm::vec3& c) {
        return glm::vec3(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b));
    };

    for (int r = 0; r < static_cast<int>(_rgbLUTDepth); ++r) {
        for (int g = 0; g < static_cast<int>(_rgbLUTDepth); ++g) {
            for (int b = 0; b < static_cast<int>(_rgbLUTDepth); ++b) {
                glm::vec3 targetSRGB(
                    r * invPaletteDepth,
                    g * invPaletteDepth,
                    b * invPaletteDepth
                );
                glm::vec3 targetLinear = srgbToLinearVec(targetSRGB);

                float minError = FLT_MAX;
                int bestFgIndex = 0, bestBgIndex = 0, bestCharIndex = 0;

                for (int fgIdx = 0; fgIdx < static_cast<int>(palette.COLOR_COUNT); ++fgIdx) {
                    glm::vec3 fgLinear = srgbToLinearVec(palette.GetRGBNormalized(fgIdx));
                    for (int bgIdx = 0; bgIdx < static_cast<int>(palette.COLOR_COUNT); ++bgIdx) {
                        glm::vec3 bgLinear = srgbToLinearVec(palette.GetRGBNormalized(bgIdx));
                        for (int charIdx = 0; charIdx < static_cast<int>(_charRamp.size()); ++charIdx) {
                            float coverage = _charCoverage[charIdx];
                            glm::vec3 simLinear = coverage * fgLinear + (1.0f - coverage) * bgLinear;
                            glm::vec3 diff = targetLinear - simLinear;
                            float error = 0.2126f * diff.r * diff.r
                                + 0.7152f * diff.g * diff.g
                                + 0.0722f * diff.b * diff.b;
                            if (error < minError) {
                                minError = error;
                                bestFgIndex = fgIdx;
                                bestBgIndex = bgIdx;
                                bestCharIndex = charIdx;
                            }
                        }
                    }
                }

                int index = (r * _rgbLUTDepth * _rgbLUTDepth) + (g * _rgbLUTDepth) + b;
                wchar_t glyph = _charRamp[bestCharIndex];
                unsigned short fgColor = static_cast<unsigned short>(palette.GetFgColor(bestFgIndex));
                unsigned short bgColor = static_cast<unsigned short>(palette.GetBgColor(bestBgIndex));
                unsigned short combinedColor = fgColor | bgColor;
                if (fgColor > 0xF || bgColor > 0xF0) {
                    fgColor = fgColor & 0xF;
                    bgColor = bgColor & 0xF0;
                    combinedColor = fgColor | bgColor;
                }
                _colorLUT[index] = {glyph, combinedColor};
            }
        }
    }
}

CHAR_INFO Renderer::GetCharInfo(const glm::ivec3& rgb) {
    if (_colorLUTState == ColorLUTState::NotComputed) {
        PrecomputeColorLUT();
    }

    if (_colorLUTState == ColorLUTState::Monochrome) {
        float L = PaletteUtil::sRGB255_Luminance(rgb);
        size_t idx = MonochromeLuminanceToIndex(L);
        return _monochromeLUT[idx].second;
    }

    auto to16 = [](int v) { return (v * 15 + 127) / 255; };
    const int r = to16(rgb.r), g = to16(rgb.g), b = to16(rgb.b);
    const int index = (r * _rgbLUTDepth * _rgbLUTDepth) + (g * _rgbLUTDepth) + b;
    return _colorLUT[index];
}

// =============================================================================
// RENDER SETTINGS - RENDERING OPTIONS
// =============================================================================

void Renderer::SetWireframe(bool wireframe) {
    _wireframe = wireframe;
}

bool Renderer::GetWireframe() const {
    return _wireframe;
}

void Renderer::SetBackfaceCulling(bool backfaceCulling) {
    _backface_culling = backfaceCulling;
}

bool Renderer::GetBackfaceCulling() const {
    return _backface_culling;
}

void Renderer::SetCCW(bool ccw) {
    _ccw = ccw;
}

bool Renderer::GetCCW() const {
    return _ccw;
}

void Renderer::SetBackgroundCol(const glm::ivec3& col) {
    _background_col = glm::clamp(col, glm::ivec3(0), glm::ivec3(255));
}

glm::ivec3 Renderer::GetBackgroundCol() const {
    return _background_col;
}

// =============================================================================
// RENDER SETTINGS - ANTIALIASING
// =============================================================================

bool Renderer::GetAntialiasing() const {
    return _antialiasing;
}

int Renderer::GetAntialiasingsamples() const {
    return _antialiasing_samples;
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

void Renderer::SetDiagnosticsEnabled(bool enabled) {
    _diagnostics_enabled = enabled;
}

bool Renderer::GetDiagnosticsEnabled() const {
    return _diagnostics_enabled;
}

void Renderer::ResetDiagnostics() {
    if (!_diagnostics_enabled) return;
}

void Renderer::LogDiagnostics() const {
    if (!_diagnostics_enabled) return;
}

void Renderer::EndColBuffFrame() {
    // Resolve MSAA render target to non-MSAA texture for CPU download
    D3D11_TEXTURE2D_DESC rtDesc;
    _renderTarget->GetDesc(&rtDesc);
    
    if (rtDesc.SampleDesc.Count > 1) {
        // MSAA is enabled - resolve to non-MSAA texture
        _context->ResolveSubresource(
            _resolvedTexture.Get(),     // Destination (non-MSAA)
            0,                           // Dest subresource
            _renderTarget.Get(),         // Source (MSAA)
            0,                           // Source subresource
            DXGI_FORMAT_R8G8B8A8_UNORM  // Format
        );
    } else {
        // No MSAA - just copy render target to resolved texture
        _context->CopyResource(_resolvedTexture.Get(), _renderTarget.Get());
    }
    
    // Present for RenderDoc (does nothing if no swap chain)
    if (_debugSwapChain) {
        _debugSwapChain->Present(0, 0);
    }

    DownloadFramebuffer();

    OverwritePxBuffWithColBuff();
}

// =========================================================================
// Custom Shader/Material System Implementation
// =========================================================================

void Renderer::BindShaderProgram(ShaderProgram* program) {
    if (!_initialized) return;
    
    _boundShaderProgram = program;
    
    if (program && program->IsValid()) {
        // Bind custom shaders and input layout
        _context->VSSetShader(program->_vertexShader->_vertexShader.Get(), nullptr, 0);
        _context->PSSetShader(program->_pixelShader->_pixelShader.Get(), nullptr, 0);
        _context->IASetInputLayout(program->_inputLayout.Get());
    } else {
        // Revert to default shaders
        UnbindShaderProgram();
    }
}

void Renderer::UnbindShaderProgram() {
    if (!_initialized) return;
    
    _boundShaderProgram = nullptr;
    _boundMaterial = nullptr;
}

void Renderer::BindMaterial(Material* material) {
    if (!_initialized || !material) return;
    
    _boundMaterial = material;
    
    // Bind shader program
    auto program = material->GetShaderProgram();
    if (program) {
        BindShaderProgram(program.get());
    }
    
    // Bind textures
    for (const auto& slot : material->_textureSlots) {
        if (slot.texture) {
            BindTexture(slot.texture, slot.slot);
        } else if (slot.textureArray) {
            BindTextureArray(slot.textureArray, slot.slot);
        }
    }
    
    // Upload constant buffer if dirty
    UploadMaterialConstants(material);
}

void Renderer::UploadMaterialConstants(Material* material) {
    if (!material || !_initialized) return;
    
    // Update material's packed constant buffer data
    material->UpdateConstantBufferData();
    
    auto program = material->GetShaderProgram();
    if (!program) return;
    
    const auto& layout = program->GetUniformLayout();
    if (layout.GetSize() == 0) return;
    
    // Create or update material's GPU constant buffer
    if (!material->_constantBufferInitialized) {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = layout.GetSize();
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        HRESULT hr = _device->CreateBuffer(&cbDesc, nullptr, &material->_constantBuffer);
        if (FAILED(hr)) {
            Logger::Error("Failed to create material constant buffer");
            return;
        }
        material->_constantBufferInitialized = true;
    }
    
    // Map and upload data
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(material->_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, material->_constantBufferData.data(), layout.GetSize());
        _context->Unmap(material->_constantBuffer.Get(), 0);
    }
    
    // Bind to both vertex and pixel shader stages
    _context->VSSetConstantBuffers(0, 1, material->_constantBuffer.GetAddressOf());
    _context->PSSetConstantBuffers(0, 1, material->_constantBuffer.GetAddressOf());
}

} // namespace ASCIIgL