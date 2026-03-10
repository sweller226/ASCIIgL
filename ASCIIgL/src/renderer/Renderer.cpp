#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <execution>
#include <limits>
#include <numeric>
#include <sstream>
#include <mutex>
#include <thread>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
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

void Renderer::OverwritePxBuffWithColBuff_Monochrome(std::vector<CHAR_INFO>& pixelBuffer, int width, size_t bufferSize) {
    if (!_monochromeDitherEnabled) {
        for (size_t i = 0; i < bufferSize; ++i) {
            const auto& c = _color_buffer[i];
            float L = PaletteUtil::sRGB255_Luminance(glm::ivec3(c.r, c.g, c.b));
            size_t idx = MonochromeLuminanceToIndex(L);
            pixelBuffer[i] = _monochromeLUT[idx].second;
        }
        return;
    }

    // 4x4 Bayer ordered dithering in LUT-index space.
    static constexpr uint8_t BAYER4[16] = {
        0,  8,  2, 10,
        12, 4, 14, 6,
        3, 11, 1, 9,
        15, 7, 13, 5
    };

    const float Lmin = _monochromeLUT.front().first;
    const float Lmax = _monochromeLUT.back().first;
    const float denom = Lmax - Lmin;

    for (size_t i = 0; i < bufferSize; ++i) {
        const auto& c = _color_buffer[i];
        float L = PaletteUtil::sRGB255_Luminance(glm::ivec3(c.r, c.g, c.b));

        size_t idx = 0;
        if (!(denom > 1e-8f)) {
            idx = 0;
        } else if (L <= Lmin) {
            idx = 0;
        } else if (L >= Lmax) {
            idx = _monochromeLUTSize - 1;
        } else {
            const int x = static_cast<int>(i % static_cast<size_t>(width));
            const int y = static_cast<int>(i / static_cast<size_t>(width));
            const float threshold = (static_cast<float>(BAYER4[((y & 3) << 2) | (x & 3)]) + 0.5f) * (1.0f / 16.0f);

            const float t = (L - Lmin) / denom; // in (0,1)
            const float fIdx = t * static_cast<float>(_monochromeLUTSize - 1);
            const float baseF = std::floor(fIdx);
            const float frac = fIdx - baseF;
            size_t base = static_cast<size_t>(baseF);

            idx = base + ((frac > threshold) ? 1u : 0u);
            if (idx >= _monochromeLUTSize) idx = _monochromeLUTSize - 1;
        }

        pixelBuffer[i] = _monochromeLUT[idx].second;
    }
}

void Renderer::OverwritePxBuffWithColBuff_MultiColor(std::vector<CHAR_INFO>& pixelBuffer, size_t bufferSize) {
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

void Renderer::OverwritePxBuffWithColBuff() {
    auto& screen = Screen::GetInst();
    auto& pixelBuffer = screen.GetPixelBuffer();
    const size_t bufferSize = _color_buffer.size();

    if (_colorLUTState == ColorLUTState::NotComputed) {
        PrecomputeColorLUT();
    }

    if (_colorLUTState == ColorLUTState::Monochrome) {
        OverwritePxBuffWithColBuff_Monochrome(pixelBuffer, screen.GetWidth(), bufferSize);
        return;
    }

    OverwritePxBuffWithColBuff_MultiColor(pixelBuffer, bufferSize);
}

// =============================================================================
// CHAR COVERAGE (loaded from JSON to match char_coverage tool)
// =============================================================================

// Character ramp order must match tools/CharCoverage DEFAULT_CHARS (coverages array index = ramp index).
static const wchar_t CHAR_RAMP_DEFAULT[] =
    L" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890.:,;'\"(!?)+-*/=\"";

static void ApplyFallbackRamp(std::vector<wchar_t>& ramp, std::vector<float>& coverage, const wchar_t* chars) {
    ramp.clear();
    coverage.clear();
    for (const wchar_t* p = chars; *p; ++p) {
        ramp.push_back(*p);
        coverage.push_back(0.5f);
    }
}

/// Subsample current ramp to K evenly spaced indices (for fallback when all coverage is equal).
static void SubsampleRampByIndex(std::vector<wchar_t>& ramp, std::vector<float>& coverage, int K) {
    const size_t n = ramp.size();
    if (K <= 0 || static_cast<size_t>(K) >= n) return;
    std::vector<wchar_t> out(static_cast<size_t>(K));
    std::vector<float> outCov(static_cast<size_t>(K));
    for (int i = 0; i < K; ++i) {
        size_t j = (K > 1) ? (static_cast<size_t>(i) * (n - 1)) / static_cast<size_t>(K - 1) : n / 2;
        out[static_cast<size_t>(i)] = ramp[j];
        outCov[static_cast<size_t>(i)] = coverage[j];
    }
    ramp = std::move(out);
    coverage = std::move(outCov);
}

void Renderer::LoadCharCoverageFromJson(const wchar_t* charRamp, int charRampCount) {
    _charRamp.clear();
    _charCoverage.clear();
    _colorLUTState = ColorLUTState::NotComputed;

    const bool useCustomRamp = (charRamp && *charRamp);
    const wchar_t* rampToUse = useCustomRamp ? charRamp : CHAR_RAMP_DEFAULT;

    float fontSize = Screen::GetInst().GetFontSize();

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
        ApplyFallbackRamp(_charRamp, _charCoverage, rampToUse);
        if (!useCustomRamp && charRampCount > 0)
            SubsampleRampByIndex(_charRamp, _charCoverage, charRampCount);
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(file);
    } catch (const std::exception& e) {
        Logger::Warning(std::string("[Renderer] Failed to parse coverage_cleartype.json: ") + e.what());
        ApplyFallbackRamp(_charRamp, _charCoverage, rampToUse);
        if (!useCustomRamp && charRampCount > 0)
            SubsampleRampByIndex(_charRamp, _charCoverage, charRampCount);
        return;
    }
    file.close();

    if (!j.contains("intervals") || !j["intervals"].is_array() || j["intervals"].empty()) {
        Logger::Warning("[Renderer] coverage_cleartype.json has no intervals.");
        ApplyFallbackRamp(_charRamp, _charCoverage, rampToUse);
        if (!useCustomRamp && charRampCount > 0)
            SubsampleRampByIndex(_charRamp, _charCoverage, charRampCount);
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
        ApplyFallbackRamp(_charRamp, _charCoverage, rampToUse);
        if (!useCustomRamp && charRampCount > 0)
            SubsampleRampByIndex(_charRamp, _charCoverage, charRampCount);
        return;
    }

    // Build char -> coverage map from JSON (chars[i] -> coverages[i])
    std::unordered_map<unsigned, float> charToCoverage;
    const size_t jsonN = coverages.size();
    const bool hasChars = j.contains("chars") && j["chars"].is_array() && j["chars"].size() >= jsonN;
    for (size_t i = 0; i < jsonN; ++i) {
        unsigned cp = hasChars ? j["chars"][i].get<unsigned>() : static_cast<unsigned>(CHAR_RAMP_DEFAULT[i]);
        charToCoverage[cp] = coverages[i].get<float>();
    }

    // Fill ramp and coverage from the requested ramp, looking up coverage from JSON
    for (const wchar_t* p = rampToUse; *p; ++p) {
        wchar_t ch = *p;
        unsigned cp = static_cast<unsigned>(ch);
        auto it = charToCoverage.find(cp);
        float cov = (it != charToCoverage.end()) ? it->second : 0.5f;
        _charRamp.push_back(ch);
        _charCoverage.push_back(cov);
    }

    // Sort ramp by coverage ascending (low-coverage chars first, high last)
    const size_t n = _charRamp.size();
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

    // When using default ramp (no custom string), optionally subsample to charRampCount chars with evenly spaced coverage
    if (!useCustomRamp && charRampCount > 0) {
        const size_t n = _charRamp.size();
        const int K = charRampCount;
        if (n > 0 && static_cast<size_t>(K) < n) {
            const float c_min = _charCoverage[0];
            const float c_max = _charCoverage[n - 1];
            std::vector<size_t> chosen;
            chosen.reserve(static_cast<size_t>(K));
            for (int i = 0; i < K; ++i) {
                float target = (K > 1) ? (c_min + (c_max - c_min) * static_cast<float>(i) / (K - 1)) : (0.5f * (c_min + c_max));
                size_t best = 0;
                float bestDist = std::abs(_charCoverage[0] - target);
                for (size_t j = 1; j < n; ++j) {
                    float d = std::abs(_charCoverage[j] - target);
                    if (d < bestDist) {
                        bestDist = d;
                        best = j;
                    }
                }
                // If this index was already chosen, pick next closest that isn't chosen (keep distinct chars)
                auto alreadyChosen = [&chosen](size_t idx) {
                    return std::find(chosen.begin(), chosen.end(), idx) != chosen.end();
                };
                if (alreadyChosen(best)) {
                    bestDist = std::numeric_limits<float>::max();
                    for (size_t j = 0; j < n; ++j) {
                        if (alreadyChosen(j)) continue;
                        float d = std::abs(_charCoverage[j] - target);
                        if (d < bestDist) {
                            bestDist = d;
                            best = j;
                        }
                    }
                }
                chosen.push_back(best);
            }
            // Sort chosen by coverage so ramp stays ascending
            std::sort(chosen.begin(), chosen.end(), [this](size_t a, size_t b) { return _charCoverage[a] < _charCoverage[b]; });
            std::vector<wchar_t> subRamp(static_cast<size_t>(K));
            std::vector<float> subCoverage(static_cast<size_t>(K));
            for (int i = 0; i < K; ++i) {
                subRamp[static_cast<size_t>(i)] = _charRamp[chosen[static_cast<size_t>(i)]];
                subCoverage[static_cast<size_t>(i)] = _charCoverage[chosen[static_cast<size_t>(i)]];
            }
            _charRamp = std::move(subRamp);
            _charCoverage = std::move(subCoverage);
        }
    }

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

    const float denom = (_monochromeLUTSize > 1) ? static_cast<float>(_monochromeLUTSize - 1) : 1.0f;
    for (size_t i = 0; i < _monochromeLUTSize; ++i) {
        float t = static_cast<float>(i) / denom;
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

    // Make sure LUT is sorted by target luminance (ascending)
    std::sort(_monochromeLUT.begin(), _monochromeLUT.end(),
              [](const std::pair<float, CHAR_INFO>& a, const std::pair<float, CHAR_INFO>& b) {
                  return a.first < b.first;
              });
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

void Renderer::SetMaxAnisotropy(int level) {
    if (level <= 1) level = 1;
    else if (level <= 2) level = 2;
    else if (level <= 4) level = 4;
    else if (level <= 8) level = 8;
    else level = 16;

    if (level == _maxAnisotropy) return;

    _maxAnisotropy = level;

    if (_initialized) {
        _samplerAnisotropic.Reset();
        if (!CreateAnisotropicSampler()) {
            Logger::Error("[Renderer] Failed to recreate anisotropic sampler at " + std::to_string(_maxAnisotropy) + "x");
        }
    }
}

int Renderer::GetMaxAnisotropy() const {
    return _maxAnisotropy;
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

    {
        ASCIIgL::PROFILE_SCOPE("Renderer.DownloadFramebuffer");
        DownloadFramebuffer();
    }

    {
        ASCIIgL::PROFILE_SCOPE("Renderer.OverwritePxBuffWithColBuff");
        OverwritePxBuffWithColBuff();
    }

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
    
    // Bind textures (material's per-slot sampler type is used)
    for (const auto& slot : material->_textureSlots) {
        if (slot.texture) {
            BindTexture(slot.texture, slot.slot, slot.samplerType);
        } else if (slot.textureArray) {
            BindTextureArray(slot.textureArray, slot.slot, slot.samplerType);
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