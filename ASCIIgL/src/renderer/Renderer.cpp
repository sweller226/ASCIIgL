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

// =============================================================================
// LIFECYCLE MANAGEMENT
// =============================================================================

Renderer::~Renderer() {
    Shutdown();
    _color_buffer.clear();
}

void Renderer::Initialize(bool antialiasing, int antialiasing_samples) {
    _antialiasing = antialiasing;
    _antialiasing_samples = antialiasing_samples;
    
    if (!_initialized) {
        Logger::Info("Initializing Renderer...");
    } else {
        Logger::Warning("Renderer is already initialized!");
        return;
    }

    if (!Screen::GetInst().IsInitialized()) {
        Logger::Error("Renderer: Screen must be initialized before creating Renderer.");
        throw std::runtime_error("Renderer: Screen must be initialized before creating Renderer.");
    }
    auto& screen = Screen::GetInst();
    _color_buffer.resize(screen.GetWidth() * screen.GetHeight());

    Logger::Info("[Renderer] Initializing DirectX 11...");

    if (!InitializeDevice()) {
        Logger::Error("[Renderer] Failed to initialize device");
        return;
    }

    if (!InitializeRenderTarget()) {
        Logger::Error("[Renderer] Failed to initialize render target");
        return;
    }

    Logger::Info("[Renderer] Initializing depth stencil...");
    if (!InitializeDepthStencil()) {
        Logger::Error("[Renderer] Failed to initialize depth stencil");
        return;
    }

    if (!InitializeSamplers()) {
        Logger::Error("[Renderer] Failed to initialize samplers");
        return;
    }

    if (!InitializeRasterizerStates()) {
        Logger::Error("[Renderer] Failed to initialize rasterizer states");
        return;
    }

    Logger::Info("[Renderer] Initializing blend states...");
    if (!InitializeBlendStates()) {
        Logger::Error("[Renderer] Failed to initialize blend states");
        return;
    }

    if (!InitializeStagingTexture()) {
        Logger::Error("[Renderer] Failed to initialize staging texture");
        return;
    }

    if (!InitializeDebugSwapChain()) {
        Logger::Error("[Renderer] Failed to initialize debug swap chain (non-fatal)");
        // Non-fatal - continue without swap chain
    }

    // Pipeline state (depth + blend) is set in BeginColBuffFrame when RTV is bound.

    _initialized = true;
    Logger::Debug("Renderer initialization complete - Device, Shaders, Buffers ready");
}

bool Renderer::IsInitialized() const {
    return _initialized;
}

// =============================================================================
// HIGH-LEVEL DRAWING API - MESHES AND MODELS
// =============================================================================

void Renderer::DrawMesh(const Mesh* mesh) {
    if (!_initialized || !mesh) {
        if (!mesh) {
            Logger::Error("DrawMesh: mesh is nullptr!");
        }
        return;
    }
    
    // Get or create GPU buffer cache for this mesh
    GPUMeshCache* cache = GetOrCreateMeshCache(mesh);
    if (!cache || !cache->vertexBuffer) return;
    
    // Bind cached vertex buffer with mesh's format stride
    UINT stride = mesh->GetVertFormat().GetStride();
    UINT offset = 0;
    _context->IASetVertexBuffers(0, 1, cache->vertexBuffer.GetAddressOf(), &stride, &offset);
    
    // Draw with or without indices
    if (cache->indexBuffer && cache->indexCount > 0) {
        _context->IASetIndexBuffer(cache->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        _context->DrawIndexed(static_cast<UINT>(cache->indexCount), 0, 0);
    } else {
        _context->Draw(static_cast<UINT>(cache->vertexCount), 0);
    }
}

void Renderer::DrawModel(const Model& ModelObj) {
    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(ModelObj.meshes[i]);
    }
}

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

// =============================================================================
// TEST/DEBUG RENDERING FUNCTIONS
// =============================================================================

void Renderer::TestRenderFont() {
    Screen& screen = Screen::GetInst();
    int screenWidth = screen.GetWidth();
    int screenHeight = screen.GetHeight();
    
    // Get the number of characters in the ramp
    const int numChars = static_cast<int>(_charRamp.size());
    
    // Calculate grid dimensions (square-ish grid)
    int gridCols = static_cast<int>(std::ceil(std::sqrt(numChars)));
    int gridRows = static_cast<int>(std::ceil(static_cast<float>(numChars) / gridCols));
    
    // Calculate patch size for each character
    int patchWidth = screenWidth / gridCols;
    int patchHeight = screenHeight / gridRows;
    
    // Combined foreground and background color
    Palette& palette = screen.GetPalette();
    unsigned short combinedColor = palette.GetFgColor(6) | palette.GetBgColor(11);
    
    // Fill screen with character patches
    for (int charIdx = 0; charIdx < numChars; ++charIdx) {
        // Calculate which patch this character belongs to
        int patchX = charIdx % gridCols;
        int patchY = charIdx / gridCols;
        
        // Calculate the bounds of this patch
        int startX = patchX * patchWidth;
        int startY = patchY * patchHeight;
        int endX = (patchX == gridCols - 1) ? screenWidth : startX + patchWidth;  // Handle remainder
        int endY = (patchY == gridRows - 1) ? screenHeight : startY + patchHeight; // Handle remainder
        
        // Fill the entire patch with this character
        wchar_t currentChar = _charRamp[charIdx];
        CHAR_INFO charInfo;

        charInfo.Char.UnicodeChar = currentChar;
        charInfo.Attributes = combinedColor;
        
        for (int y = startY; y < endY; ++y) {
            for (int x = startX; x < endX; ++x) {
                screen.PlotPixel(glm::vec2(x, y), charInfo);
            }
        }
    }
}

void Renderer::TestRenderColorDiscrete() {
    Screen& screen = Screen::GetInst();
    int screenWidth = screen.GetWidth();
    int screenHeight = screen.GetHeight();

    // 16 colors, arrange in a 4x4 grid
    const int numColors = Palette::COLOR_COUNT;
    const int gridCols = 4;
    const int gridRows = 4;

    int patchWidth = screenWidth / gridCols;
    int patchHeight = screenHeight / gridRows;

    Palette& palette = screen.GetPalette();

    for (int colorIdx = 0; colorIdx < numColors; ++colorIdx) {
        int patchX = colorIdx % gridCols;
        int patchY = colorIdx / gridCols;

        int startX = patchX * patchWidth;
        int startY = patchY * patchHeight;
        int endX = (patchX == gridCols - 1) ? screenWidth : startX + patchWidth;
        int endY = (patchY == gridRows - 1) ? screenHeight : startY + patchHeight;

        // Use a solid block character for visibility
        CHAR_INFO charInfo;
        charInfo.Char.UnicodeChar = 'B';
        charInfo.Attributes = palette.GetFgColor(colorIdx) | palette.GetBgColor(colorIdx);

        for (int y = startY; y < endY; ++y) {
            for (int x = startX; x < endX; ++x) {
                screen.PlotPixel(glm::vec2(x, y), charInfo);
            }
        }
    }
}

void Renderer::TestRenderColorContinuous() {
    Screen& screen = Screen::GetInst();
    
    // Display all 4096 colors in a 64x64 grid
    // Each position (x,y) maps directly to a unique RGB combination
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            // Linear index from 0 to 4095
            int linearIndex = y * 64 + x;
            
            // Reverse the 3D indexing to get RGB values
            // index = (r * 16 * 16) + (g * 16) + b
            int r = linearIndex / (_rgbLUTDepth * _rgbLUTDepth);  // High order
            int g = (linearIndex / _rgbLUTDepth) % _rgbLUTDepth;  // Middle order
            int b = linearIndex % _rgbLUTDepth;                    // Low order
            
            // Reconstruct the LUT index
            int lutIndex = (r * _rgbLUTDepth * _rgbLUTDepth) + (g * _rgbLUTDepth) + b;
            CHAR_INFO charInfo = _colorLUT[lutIndex];

            screen.PlotPixel(glm::vec2(x + 10 + int(x/16), y + 10 + int(y/16)), charInfo);
        }
    }
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
// GPU Initialization Methods
// =========================================================================

bool Renderer::InitializeDevice() {
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Use hardware acceleration
        nullptr,                    // No software module
        createDeviceFlags,          // Device flags
        featureLevels,              // Feature levels to try
        ARRAYSIZE(featureLevels),   // Number of feature levels
        D3D11_SDK_VERSION,          // SDK version
        &_device,                   // Output device
        &featureLevel,              // Chosen feature level
        &_context                   // Output context
    );

    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] D3D11CreateDevice failed: 0x" + ss.str());
        return false;
    }

    std::string featureLevelStr = (featureLevel == D3D_FEATURE_LEVEL_11_1) ? "11.1" : "11.0";
    Logger::Info("[Renderer] Device created with feature level: " + featureLevelStr);

    return true;
}

bool Renderer::InitializeRenderTarget() {
    // Get MSAA settings
    bool useMSAA = GetAntialiasing();
    int msaaSamples = GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    UINT qualityLevel = 0;
    
    if (useMSAA) {
        sampleCount = std::clamp(msaaSamples, 1, 8);  // Clamp to 1-8 samples
        
        // Check MSAA quality support
        UINT numQualityLevels = 0;
        _device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, sampleCount, &numQualityLevels);
        if (numQualityLevels > 0) {
            qualityLevel = 0; 
        } else {
            Logger::Warning("[Renderer] " + std::to_string(sampleCount) + " x MSAA not supported, falling back to 1x");
            sampleCount = 1;
        }
    }
    
    Logger::Debug("[Renderer] Creating render target with " + std::to_string(sampleCount) + "x MSAA");
    
    // Create MSAA render target texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = Screen::GetInst().GetWidth();
    texDesc.Height = Screen::GetInst().GetHeight();
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = sampleCount;
    texDesc.SampleDesc.Quality = qualityLevel;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;

    HRESULT hr = _device->CreateTexture2D(&texDesc, nullptr, &_renderTarget);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create render target texture: 0x" + ss.str());
        return false;
    }

    // Create render target view
    hr = _device->CreateRenderTargetView(_renderTarget.Get(), nullptr, &_renderTargetView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create render target view: 0x" + ss.str());
        return false;
    }
    
    // Create resolved (non-MSAA) texture for CPU download
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.BindFlags = 0;  // No binding needed, just for copying
    
    hr = _device->CreateTexture2D(&texDesc, nullptr, &_resolvedTexture);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create resolved texture: 0x" + ss.str());
        return false;
    }

    std::string msg = "[Renderer] Render target initialized: " + 
        std::to_string(Screen::GetInst().GetWidth()) + "x" + 
        std::to_string(Screen::GetInst().GetHeight()) + 
        (useMSAA ? (" with " + std::to_string(sampleCount) + "x MSAA") : " without MSAA");
    Logger::Info(msg);

    return true;
}

bool Renderer::InitializeDepthStencil() {
    // Get MSAA settings (must match render target)
    bool useMSAA = GetAntialiasing();
    int msaaSamples = GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    if (useMSAA) {
        sampleCount = std::clamp(msaaSamples, 1, 8);
    }
    
    // Create depth stencil texture (must match render target MSAA settings)
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = Screen::GetInst().GetWidth();
    depthDesc.Height = Screen::GetInst().GetHeight();
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = sampleCount;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = _device->CreateTexture2D(&depthDesc, nullptr, &_depthStencilBuffer);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create depth stencil buffer: 0x" + ss.str());
        return false;
    }

    // Create depth stencil view
    hr = _device->CreateDepthStencilView(_depthStencilBuffer.Get(), nullptr, &_depthStencilView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create depth stencil view: 0x" + ss.str());
        return false;
    }

    // Create depth stencil state
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;  // Standard depth test: closer objects win
    dsDesc.StencilEnable = FALSE;

    hr = _device->CreateDepthStencilState(&dsDesc, &_depthStencilState);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create depth stencil state: 0x" + ss.str());
        return false;
    }

    // No-depth state (depth test disabled)
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = _device->CreateDepthStencilState(&dsDesc, &_depthStencilStateNoTest);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create no-depth stencil state: 0x" + ss.str());
        return false;
    }

    // Depth test on, depth write off — for transparent/2D so they don't occlude geometry behind
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = _device->CreateDepthStencilState(&dsDesc, &_depthStencilStateNoWrite);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create no-write depth stencil state: 0x" + ss.str());
        return false;
    }

    return true;
}

bool Renderer::InitializeSamplers() {
    // Create point sampler with linear mipmap blending for pixel art
    // Point filtering keeps pixels sharp, linear mip blending reduces shimmering at distance
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;  // Point filtering with smooth mipmap transitions
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;  // Clamp to prevent bleeding at edges
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MipLODBias = -0.5f;  // Use slightly sharper mipmaps to reduce atlas bleeding
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = _device->CreateSamplerState(&samplerDesc, &_samplerLinear);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create sampler state: 0x" + ss.str());
        return false;
    }

    Logger::Debug("[Renderer] Created point sampler with linear mipmap blending for pixel art");
    return true;
}

bool Renderer::InitializeRasterizerStates() {
    D3D11_RASTERIZER_DESC desc = {};
    desc.DepthClipEnable = TRUE;  // Enable depth clipping

    // Create all 8 combinations: wireframe(0/1) + cull(0/2) + ccw(0/4)
    for (int i = 0; i < 8; ++i) {
        bool wireframe = (i & 1) != 0;      // Bit 0: wireframe
        bool cull = (i & 2) != 0;           // Bit 1: backface culling
        bool ccw = (i & 4) != 0;            // Bit 2: counter-clockwise winding

        desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
        desc.CullMode = cull ? D3D11_CULL_BACK : D3D11_CULL_NONE;
        desc.FrontCounterClockwise = ccw ? TRUE : FALSE;

        HRESULT hr = _device->CreateRasterizerState(&desc, &_rasterizerStates[i]);
        if (FAILED(hr)) {
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[Renderer] Failed to create rasterizer state " + std::to_string(i) + ": 0x" + ss.str());
            return false;
        }
    }

    return true;
}

bool Renderer::InitializeBlendStates() {
    // Opaque: no blending (default for 3D)
    D3D11_BLEND_DESC opaqueDesc = {};
    opaqueDesc.AlphaToCoverageEnable = FALSE;
    opaqueDesc.IndependentBlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].BlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = _device->CreateBlendState(&opaqueDesc, &_blendStateOpaque);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create opaque blend state: 0x" + ss.str());
        return false;
    }

    // Alpha: blend for GUI (transparent PNG regions)
    D3D11_BLEND_DESC alphaDesc = {};
    alphaDesc.AlphaToCoverageEnable = FALSE;
    alphaDesc.IndependentBlendEnable = FALSE;
    alphaDesc.RenderTarget[0].BlendEnable = TRUE;
    alphaDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alphaDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alphaDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    alphaDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    alphaDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    alphaDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alphaDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = _device->CreateBlendState(&alphaDesc, &_blendStateAlpha);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create alpha blend state: 0x" + ss.str());
        return false;
    }

    return true;
}

bool Renderer::InitializeStagingTexture() {
    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = Screen::GetInst().GetWidth();
    stagingDesc.Height = Screen::GetInst().GetHeight();
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = _device->CreateTexture2D(&stagingDesc, nullptr, &_stagingTexture);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create staging texture: 0x" + ss.str());
        return false;
    }

    return true;
}

bool Renderer::InitializeDebugSwapChain() {
    // Get console window handle for minimal swap chain
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) {
        Logger::Error("[Renderer] Failed to get console window handle");
        return false;
    }

    // Create a minimal 1x1 swap chain for RenderDoc
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = 1;
    swapChainDesc.BufferDesc.Height = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Get DXGI factory from device
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(_device.As(&dxgiDevice))) {
        Logger::Error("[Renderer] Failed to get DXGI device");
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        Logger::Error("[Renderer] Failed to get DXGI adapter");
        return false;
    }

    ComPtr<IDXGIFactory> dxgiFactory;
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory))) {
        Logger::Error("[Renderer] Failed to get DXGI factory");
        return false;
    }

    if (FAILED(dxgiFactory->CreateSwapChain(_device.Get(), &swapChainDesc, &_debugSwapChain))) {
        Logger::Error("[Renderer] Failed to create debug swap chain");
        return false;
    }

    Logger::Debug("[Renderer] Debug swap chain created successfully");
    return true;
}

// =========================================================================
// Framebuffer Download (GPU -> CPU)
// =========================================================================

void Renderer::DownloadFramebuffer()
{
    Logger::Debug("[Renderer] Downloading framebuffer from GPU to CPU...");

    if (!_initialized) {
        Logger::Warning("[Renderer] DownloadFramebuffer called before initialization!");
        return;
    };

    Logger::Debug("DownloadFramebuffer: Copying GPU framebuffer to CPU");

    const int width = Screen::GetInst().GetWidth();
    const int height = Screen::GetInst().GetHeight();

    // Ensure color buffer is correct size
    if (_color_buffer.size() != width * height) {
        _color_buffer.resize(width * height);
    }

    // Copy resolved texture to staging texture (resolved texture is always non-MSAA)
    _context->CopyResource(_stagingTexture.Get(), _resolvedTexture.Get());

    // Map staging texture
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        Logger::Warning("Failed to map staging texture: 0x" + std::to_string(hr));
        return;
    }

    // Copy pixels to color buffer (keep 0-255, convert to 0-15 at LUT lookup)
    uint8_t* src = static_cast<uint8_t*>(mapped.pData);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = src + y * mapped.RowPitch + x * 4; // RGBA
            int index = y * width + x;
            
            // Keep 0-255 precision in color buffer
            _color_buffer[index] = glm::ivec4(
                pixel[0],  // R (0-255)
                pixel[1],  // G (0-255)
                pixel[2],  // B (0-255)
                pixel[3]   // A (0-255)
            );
        }
    }

    _context->Unmap(_stagingTexture.Get(), 0);
    
    // Sample some pixels to verify we have actual data
    int nonBlackPixels = 0;
    for (int i = 0; i < width * height; i++) {
        auto& pixel = _color_buffer[i];
        if (pixel.r != 0 || pixel.g != 0 || pixel.b != 0) {
            nonBlackPixels++;
        }
    }
    Logger::Debug("DownloadFramebuffer: Successfully downloaded " + std::to_string(width * height) + " pixels, " + std::to_string(nonBlackPixels) + " non-black");
    
    // Sample center pixel
    int centerIdx = (height / 2) * width + (width / 2);
    auto& centerPixel = _color_buffer[centerIdx];
    Logger::Debug("Center pixel color: R=" + std::to_string(centerPixel.r) + " G=" + std::to_string(centerPixel.g) + " B=" + std::to_string(centerPixel.b));
}

// =========================================================================
// Depth and Blend State Management
// =========================================================================

void Renderer::SetDepthTestEnabled(bool enabled) {
    if (!_initialized) return;
    ID3D11DepthStencilState* state = enabled ? _depthStencilState.Get() : _depthStencilStateNoTest.Get();
    _context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetDepthWriteEnabled(bool enabled) {
    if (!_initialized) return;
    ID3D11DepthStencilState* state = enabled ? _depthStencilState.Get() : _depthStencilStateNoWrite.Get();
    _context->OMSetDepthStencilState(state, 0);
}

void Renderer::SetBlendEnabled(bool enabled) {
    if (!_initialized) return;
    ID3D11BlendState* state = enabled ? _blendStateAlpha.Get() : _blendStateOpaque.Get();
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    _context->OMSetBlendState(state, blendFactor, 0xFFFFFFFF);
}

// =========================================================================
// Cleanup
// =========================================================================

void Renderer::Shutdown()
{
    if (!_initialized) return;

    // Release all COM objects (ComPtr handles this automatically)
    _textureCache.clear();  // Clear texture cache
    _currentTextureSRV.Reset();
    _stagingTexture.Reset();
    
    // Release sampler
    _samplerLinear.Reset();
    
    // Release all rasterizer states
    for (auto& state : _rasterizerStates) {
        state.Reset();
    }

    _blendStateAlpha.Reset();
    _blendStateOpaque.Reset();
    _depthStencilStateNoWrite.Reset();
    _depthStencilStateNoTest.Reset();
    _depthStencilState.Reset();
    _depthStencilView.Reset();
    _depthStencilBuffer.Reset();
    _renderTargetView.Reset();
    _resolvedTexture.Reset();
    _renderTarget.Reset();
    _context.Reset();
    _device.Reset();

    _initialized = false;
    Logger::Debug("[Renderer] Shutdown complete");
}

// =========================================================================
// Per-Mesh GPU Buffer Cache Management
// =========================================================================

Renderer::GPUMeshCache* Renderer::GetOrCreateMeshCache(const Mesh* mesh) {
    if (!mesh) return nullptr;
    
    // Check if cache already exists
    if (mesh->gpuBufferCache) {
        return static_cast<GPUMeshCache*>(mesh->gpuBufferCache);
    }
    
    // Create new cache
    GPUMeshCache* cache = new GPUMeshCache();
    
    // Create vertex buffer (immutable for static meshes)
    if (mesh->GetVertexDataSize() > 0) {
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = static_cast<UINT>(mesh->GetVertexDataSize());  // Size in bytes
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;  // Static - won't change
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        
        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = mesh->GetVertices().data();
        
        HRESULT hr = _device->CreateBuffer(&vbDesc, &vbData, &cache->vertexBuffer);
        if (FAILED(hr)) {
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[Renderer] Failed to create mesh vertex buffer: 0x" + ss.str());
            delete cache;
            return nullptr;
        }
        cache->vertexCount = mesh->GetVertexCount();
    }
    
    // Create index buffer if mesh is indexed
    if (!mesh->GetIndices().empty()) {
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(int) * mesh->GetIndices().size();
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        
        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = mesh->GetIndices().data();
        
        HRESULT hr = _device->CreateBuffer(&ibDesc, &ibData, &cache->indexBuffer);
        if (FAILED(hr)) {
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[Renderer] Failed to create mesh index buffer: 0x" + ss.str());
            delete cache;
            return nullptr;
        }
        cache->indexCount = mesh->GetIndices().size();
    }
    
    // Store cache in mesh
    mesh->gpuBufferCache = cache;
    
    Logger::Debug("[Renderer] Created GPU buffer cache for mesh: " + 
                  std::to_string(cache->vertexCount) + " vertices, " + 
                  std::to_string(cache->indexCount) + " indices");
    
    return cache;
}

void Renderer::ReleaseMeshCache(void* cachePtr) {
    if (!cachePtr) return;
    
    GPUMeshCache* cache = static_cast<GPUMeshCache*>(cachePtr);
    delete cache;  // ComPtr automatically releases buffers
}

void Renderer::InvalidateCachedTexture(const Texture* tex) {
    if (!tex) return;
    
    auto it = _textureCache.find(tex);
    if (it != _textureCache.end()) {
        _textureCache.erase(it);
        Logger::Debug("[Renderer] Texture cache invalidated for texture");
    }
}

// =========================================================================
// Texture Management
// =========================================================================

bool Renderer::CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv)
{
    if (!tex) return false;

    const int baseW = tex->GetWidth();
    const int baseH = tex->GetHeight();
    const uint8_t* baseData = tex->GetDataPtr();

    if (!baseData) return false;

    const bool hasCustom = tex->HasCustomMipmaps();
    const int mipCount = tex->GetMipCount();

    // -----------------------------
    // Texture data is already 0–255
    // -----------------------------
    auto GetTextureData = [&](const uint8_t* src, int w, int h) {
        size_t count = w * h * 4;
        std::vector<uint8_t> out(count);
        std::memcpy(out.data(), src, count);  // Data is already 0–255
        return out;
    };

    // -----------------------------
    // Create texture description
    // -----------------------------
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = baseW;
    desc.Height = baseH;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (hasCustom) {
        desc.MipLevels = mipCount;
        desc.MiscFlags = 0; // no auto-mips
    } else {
        desc.MipLevels = 0; // auto-generate
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create texture");
        return false;
    }

    // -----------------------------
    // Upload mip 0
    // -----------------------------
    {
        auto gpuData = GetTextureData(baseData, baseW, baseH);
        _context->UpdateSubresource(texture.Get(), 0, nullptr, gpuData.data(), baseW * 4, 0);
    }

    // -----------------------------
    // Upload custom mipmaps
    // -----------------------------
    if (hasCustom) {
        for (int level = 1; level < mipCount; ++level) {
            int w = tex->GetMipWidth(level);
            int h = tex->GetMipHeight(level);
            const uint8_t* mipData = tex->GetMipDataPtr(level);

            auto gpuData = GetTextureData(mipData, w, h);

            _context->UpdateSubresource(
                texture.Get(),
                level,
                nullptr,
                gpuData.data(),
                w * 4,
                0
            );
        }
    }

    // -----------------------------
    // Create SRV
    // -----------------------------
    hr = _device->CreateShaderResourceView(texture.Get(), nullptr, srv);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create SRV");
        return false;
    }

    // -----------------------------
    // Auto-generate GPU mipmaps if needed
    // -----------------------------
    if (!hasCustom) {
        _context->GenerateMips(*srv);
    }

    return true;
}

void Renderer::BindTexture(const Texture* tex, int slot) {
    if (!_initialized) {
        UnbindTexture(slot);
        return;
    }

    if (!tex) {
        UnbindTexture(slot);
        return;
    }

    // Check cache first
    auto it = _textureCache.find(tex);
    if (it != _textureCache.end()) {
        // Texture already uploaded - just bind it
        _currentTextureSRV = it->second;
        // Bind to specific slot
        ID3D11ShaderResourceView* srvs[] = { it->second.Get() };
        _context->PSSetShaderResources(slot, 1, srvs);
        
        ID3D11SamplerState* samplers[] = { _samplerLinear.Get() };
        _context->PSSetSamplers(slot, 1, samplers);
        return;
    }

    // First time seeing this texture - create and cache it
    ID3D11ShaderResourceView* srv = nullptr;
    if (CreateTextureFromASCIIgLTexture(tex, &srv)) {
        _currentTextureSRV.Attach(srv);
        _textureCache[tex] = _currentTextureSRV;  // Cache for future use
        
        // Bind to specific slot
        ID3D11ShaderResourceView* srvs[] = { srv };
        _context->PSSetShaderResources(slot, 1, srvs);
        
        ID3D11SamplerState* samplers[] = { _samplerLinear.Get() };
        _context->PSSetSamplers(slot, 1, samplers);
    }
}

bool Renderer::CreateTextureArraySRV(const TextureArray* texArray, ID3D11ShaderResourceView** srv) {
    if (!texArray || !texArray->IsValid()) return false;

    const int layerCount = texArray->GetLayerCount();
    const int tileSize = texArray->GetTileSize();
    const bool hasCustom = texArray->HasCustomMipmaps();
    const int mipCount = texArray->GetMipCount();

    Logger::Debug("[Renderer] Creating Texture2DArray: " + std::to_string(layerCount) + " layers, " +
                  std::to_string(tileSize) + "x" + std::to_string(tileSize) + ", " +
                  std::to_string(mipCount) + " mip levels");

    // Create texture description for Texture2DArray
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = tileSize;
    desc.Height = tileSize;
    desc.ArraySize = layerCount;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (hasCustom) {
        desc.MipLevels = mipCount;
        desc.MiscFlags = 0;
    } else {
        desc.MipLevels = 0;
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create Texture2DArray");
        return false;
    }

    // Calculate actual mip levels for GPU texture
    int actualMipLevels = mipCount;
    if (!hasCustom) {
        // When GPU auto-generates mipmaps, calculate the actual mip count
        // MipLevels = floor(log2(max(width, height))) + 1
        int size = tileSize;
        actualMipLevels = 1;
        while (size > 1) {
            size /= 2;
            actualMipLevels++;
        }
    }

    Logger::Debug("[Renderer] Actual GPU mip levels: " + std::to_string(actualMipLevels));

    // Upload layers
    for (int layer = 0; layer < layerCount; ++layer) {
        if (hasCustom) {
            // Upload all custom mip levels
            for (int mip = 0; mip < mipCount; ++mip) {
                const uint8_t* data = texArray->GetLayerData(layer, mip);
                if (!data) continue;

                int mipW = texArray->GetMipWidth(mip);
                int mipH = texArray->GetMipHeight(mip);

                UINT subresource = D3D11CalcSubresource(mip, layer, actualMipLevels);
                _context->UpdateSubresource(texture.Get(), subresource, nullptr, data, mipW * 4, 0);
            }
        } else {
            // Only upload base mip level (mip 0) - GPU will generate the rest
            const uint8_t* data = texArray->GetLayerData(layer, 0);
            if (!data) continue;

            UINT subresource = D3D11CalcSubresource(0, layer, actualMipLevels);
            _context->UpdateSubresource(texture.Get(), subresource, nullptr, data, tileSize * 4, 0);
        }
    }

    // Create SRV for Texture2DArray
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = hasCustom ? mipCount : -1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = layerCount;

    hr = _device->CreateShaderResourceView(texture.Get(), &srvDesc, srv);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create Texture2DArray SRV");
        return false;
    }

    // Auto-generate GPU mipmaps if needed
    if (!hasCustom) {
        _context->GenerateMips(*srv);
    }

    Logger::Info("[Renderer] Successfully created Texture2DArray");
    return true;
}

void Renderer::BindTextureArray(const TextureArray* texArray, int slot) {
    if (!_initialized || !texArray || !texArray->IsValid()) {
        UnbindTextureArray(slot);
        return;
    }

    ComPtr<ID3D11ShaderResourceView> srv;
    
    auto it = _textureArrayCache.find(texArray);
    if (it != _textureArrayCache.end()) {
        srv = it->second;
    } else {
        ID3D11ShaderResourceView* rawSRV = nullptr;
        if (CreateTextureArraySRV(texArray, &rawSRV)) {
            srv.Attach(rawSRV);
            _textureArrayCache[texArray] = srv;
        }
    }

    if (srv) {
        _currentTextureSRV = srv; // Note: tracking current only works well for singe slot, might need array if tracking per slot
        
        // Bind texture and sampler to pixel shader
        ID3D11ShaderResourceView* srvs[] = { srv.Get() };
        _context->PSSetShaderResources(slot, 1, srvs);
        
        ID3D11SamplerState* samplers[] = { _samplerLinear.Get() };
        _context->PSSetSamplers(slot, 1, samplers);
    }
}

void Renderer::UnbindTexture(int slot) {
    if (!_initialized) return;
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    _context->PSSetShaderResources(slot, 1, nullSRVs);
    if (slot == 0) _currentTextureSRV = nullptr;
}

void Renderer::UnbindTextureArray(int slot) {
    if (!_initialized) return;
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    _context->PSSetShaderResources(slot, 1, nullSRVs);
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