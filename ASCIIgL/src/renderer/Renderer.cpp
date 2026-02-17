#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>
#include <execution>
#include <numeric>
#include <sstream>
#include <mutex>
#include <thread>
#include <iostream>
#include <Windows.h>

#include <tbb/parallel_for_each.h>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>
#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>
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
    // DRAWING BORDERS
    DrawLinePxBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, _charRamp.back(), col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, _charRamp.back(), col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, _charRamp.back(), col);
    DrawLinePxBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, _charRamp.back(), col);
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

    // Clear render target (convert 0-255 integer color to 0-1 float)
    glm::ivec3 bgCol = GetBackgroundCol();
    float clear_color[4] = { 
        static_cast<float>(bgCol.x) / 255.0f, 
        static_cast<float>(bgCol.y) / 255.0f, 
        static_cast<float>(bgCol.z) / 255.0f, 
        1.0f 
    };
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
    
    if (!_colorLUTComputed) {
        PrecomputeColorLUT();
    }
    
    const size_t unrollFactor = 4;
    const size_t unrolledEnd = (bufferSize / unrollFactor) * unrollFactor;
    
    // Lambda to convert 0-255 to 0-15 for LUT indexing
    auto to16 = [](int v) { return (v * 15 + 127) / 255; };
    
    size_t i = 0;
    for (; i < unrolledEnd; i += unrollFactor) {
        const auto& color0 = _color_buffer[i];
        const auto& color1 = _color_buffer[i + 1];
        const auto& color2 = _color_buffer[i + 2];
        const auto& color3 = _color_buffer[i + 3];
        
        // Convert 0-255 to 0-15 for LUT indexing
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
// COLOR LOOKUP TABLE (LUT)
// =============================================================================

void Renderer::PrecomputeColorLUT() {
    Palette& palette = Screen::GetInst().GetPalette();
    if (_colorLUTComputed) return;
    
    // Use the configured palette mode
    PaletteMode mode = _paletteMode;
    
    const float invPaletteDepth = 1.0f / 15.0f;
    
    // sRGB to linear conversion (IEC 61966-2-1)
    auto srgbToLinear = [](float s) {
        return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
    };
    
    auto srgbToLinearVec = [&srgbToLinear](const glm::vec3& c) {
        return glm::vec3(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b));
    };
    
    // Precompute all possible RGB combinations
    for (int r = 0; r < static_cast<int>(_rgbLUTDepth); ++r) {
        for (int g = 0; g < static_cast<int>(_rgbLUTDepth); ++g) {
            for (int b = 0; b < static_cast<int>(_rgbLUTDepth); ++b) {
                // Convert discrete RGB to normalized [0,1] sRGB range
                glm::vec3 targetSRGB(
                    r * invPaletteDepth,
                    g * invPaletteDepth,
                    b * invPaletteDepth
                );

                float minError = FLT_MAX;
                int bestFgIndex = 0, bestBgIndex = 0, bestCharIndex = 0;

                if (mode == PaletteMode::Monochrome) {
                    // ===== MONOCHROME PATH: Compare by luminance only =====
                    // For monochrome palettes (grayscale, amber, sepia), all colors
                    // lie on the same hue line. Comparing by luminance is faster
                    // and more accurate than RGB distance.
                    
                    float targetLuminance = PaletteUtil::sRGB1_Luminance(targetSRGB);
                    
                    for (int fgIdx = 0; fgIdx < static_cast<int>(palette.COLOR_COUNT); ++fgIdx) {
                        float fgLuminance = palette.GetLuminance(fgIdx);
                        
                        for (int bgIdx = 0; bgIdx < static_cast<int>(palette.COLOR_COUNT); ++bgIdx) {
                            float bgLuminance = palette.GetLuminance(bgIdx);
                            
                            for (int charIdx = 0; charIdx < static_cast<int>(_charRamp.size()); ++charIdx) {
                                float coverage = _charCoverage[charIdx];
                                
                                // Simulate luminance as weighted blend
                                float simLuminance = coverage * fgLuminance + (1.0f - coverage) * bgLuminance;
                                float error = std::abs(targetLuminance - simLuminance);

                                if (error < minError) {
                                    minError = error;
                                    bestFgIndex = fgIdx;
                                    bestBgIndex = bgIdx;
                                    bestCharIndex = charIdx;
                                }
                            }
                        }
                    }
                } else {
                    // ===== MULTICOLOR PATH: Linearized sRGB with weighted distance =====
                    // For full-color palettes, linearize sRGB before comparison and use
                    // perceptually weighted Euclidean distance (green-sensitive).
                    
                    glm::vec3 targetLinear = srgbToLinearVec(targetSRGB);
                    
                    for (int fgIdx = 0; fgIdx < static_cast<int>(palette.COLOR_COUNT); ++fgIdx) {
                        glm::vec3 fgLinear = srgbToLinearVec(palette.GetRGBNormalized(fgIdx));
                        
                        for (int bgIdx = 0; bgIdx < static_cast<int>(palette.COLOR_COUNT); ++bgIdx) {
                            glm::vec3 bgLinear = srgbToLinearVec(palette.GetRGBNormalized(bgIdx));
                            
                            for (int charIdx = 0; charIdx < static_cast<int>(_charRamp.size()); ++charIdx) {
                                float coverage = _charCoverage[charIdx];
                                
                                // Simulate color in linear space
                                glm::vec3 simLinear = coverage * fgLinear + (1.0f - coverage) * bgLinear;
                                glm::vec3 diff = targetLinear - simLinear;
                                
                                // Weighted Euclidean distance (Rec. 709 luminance weights)
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
                }

                // Store precomputed result
                int index = (r * _rgbLUTDepth * _rgbLUTDepth) + (g * _rgbLUTDepth) + b;
                wchar_t glyph = _charRamp[bestCharIndex];
                unsigned short fgColor = static_cast<unsigned short>(palette.GetFgColor(bestFgIndex));
                unsigned short bgColor = static_cast<unsigned short>(palette.GetBgColor(bestBgIndex));
                unsigned short combinedColor = fgColor | bgColor;

                if (fgColor > 0xF || bgColor > 0xF0) {
                    // Clamp to valid range
                    fgColor = fgColor & 0xF;
                    bgColor = bgColor & 0xF0;
                    combinedColor = fgColor | bgColor;
                }

                _colorLUT[index] = {glyph, combinedColor};
            }
        }
    }

    _colorLUTComputed = true;
}

CHAR_INFO Renderer::GetCharInfo(const glm::ivec3& rgb) {
    if (!_colorLUTComputed) {
        PrecomputeColorLUT();
    }

    // Convert 0-255 to 0-15 for LUT indexing
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

void Renderer::SetPaletteMode(PaletteMode mode) {
    if (_paletteMode != mode) {
        _paletteMode = mode;
        _colorLUTComputed = false; // Invalidate LUT to force recomputation
    }
}

PaletteMode Renderer::GetPaletteMode() const {
    return _paletteMode;
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