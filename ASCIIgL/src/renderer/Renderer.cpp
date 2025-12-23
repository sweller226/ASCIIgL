#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>
#include <execution>
#include <numeric>
#include <sstream>
#include <mutex>
#include <thread>

#include <tbb/parallel_for_each.h>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>
#include <ASCIIgL/util/Profiler.hpp>

#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/RendererCPU.hpp>
#include <ASCIIgL/renderer/RendererGPU.hpp>

// =============================================================================
// LIFECYCLE MANAGEMENT
// =============================================================================

Renderer::~Renderer() {
    _color_buffer.clear();
}

void Renderer::Initialize(bool antialiasing, int antialiasing_samples, bool cpu_only) {
    _antialiasing = antialiasing;
    _antialiasing_samples = antialiasing_samples;
    _cpu_only = cpu_only;
    
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

    if (_cpu_only) {
        if (!_rendererCPU) {
            _rendererCPU = &RendererCPU::GetInst();
            _rendererCPU->Initialize();
        }
    }
    else {
        if (!_rendererGPU) {
            _rendererGPU = &RendererGPU::GetInst();
            _rendererGPU->Initialize();
        }
    }

    _initialized = true;
}

bool Renderer::IsInitialized() const {
    return _initialized;
}

// =============================================================================
// RENDERING MODE CONFIGURATION
// =============================================================================

bool Renderer::GetCpuOnly() const {
    return _cpu_only;
}

// =============================================================================
// HIGH-LEVEL DRAWING API - MESHES AND MODELS
// =============================================================================

void Renderer::DrawMesh(const Mesh* mesh) {
    if (!mesh) {
        Logger::Error("DrawMesh: mesh is nullptr!");
        return;
    }
    if (mesh->texture) {
        if (_cpu_only) {
            _rendererCPU->DrawMesh(mesh);
        } else {
            _rendererGPU->DrawMesh(mesh);
        }
    } else {
        Logger::Warning("DrawMesh: mesh texture is null");
    }
}

void Renderer::DrawMesh(const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera) {
    if (!mesh) {
        Logger::Error("DrawMesh: mesh is nullptr!");
        return;
    }
    if (mesh->texture) {
        if (_cpu_only) {
            _rendererCPU->DrawMesh(mesh, position, rotation, size, camera);
        } else {
            _rendererGPU->DrawMesh(mesh, position, rotation, size, camera);
        }
    } else {
        Logger::Warning("DrawMesh: mesh texture is null");
    }
}

void Renderer::DrawModel(const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera) {
    if (_cpu_only) {
        _rendererCPU->DrawModel(ModelObj, position, rotation, size, camera);
    } else {
        _rendererGPU->DrawModel(ModelObj, position, rotation, size, camera);
    }
}

void Renderer::DrawModel(const Model& ModelObj, const glm::mat4& model, const Camera3D& camera) {
     if (_cpu_only) {
        _rendererCPU->DrawModel(ModelObj, model, camera);
     } else {
        _rendererGPU->DrawModel(ModelObj, model, camera);
     }
}

// =============================================================================
// HIGH-LEVEL DRAWING API - 2D QUADS
// =============================================================================

void Renderer::Draw2DQuadPixelSpace(const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer) {
    if (_cpu_only) {
        _rendererCPU->Draw2DQuadPixelSpace(tex, position, rotation, size, camera, layer);
    } else {
        _rendererGPU->Draw2DQuadPixelSpace(tex, position, rotation, size, camera, layer);
    }
}

void Renderer::Draw2DQuadPercSpace(const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer) {
    if (_cpu_only) {
        _rendererCPU->Draw2DQuadPercSpace(tex, positionPerc, rotation, sizePerc, camera, layer);
    } else {
        _rendererGPU->Draw2DQuadPercSpace(tex, positionPerc, rotation, sizePerc, camera, layer);
    }
}

// =============================================================================
// RENDERING PIPELINE ENTRY POINTS
// =============================================================================

void Renderer::RenderTriangles(const std::vector<VERTEX>& vertices, const Texture* tex) {
    if (tex && (tex->GetWidth() <= 0 || tex->GetHeight() <= 0)) {
        Logger::Warning("RenderTriangles: Texture is invalid or has zero dimensions.");
        return;
    }

    if (_cpu_only) {
        RenderTrianglesCPU(vertices, tex);
    } else {
        RenderTrianglesGPU(vertices, tex);
    }
}

void Renderer::RenderTriangles(const std::vector<std::vector<VERTEX>*>& vertices, const Texture* tex) {
    if (tex && (tex->GetWidth() <= 0 || tex->GetHeight() <= 0)) {
        Logger::Warning("RenderTriangles: Texture is invalid or has zero dimensions.");
        return;
    }

    if (_cpu_only) {
        RenderTrianglesCPU(vertices, tex);
    } else {
        RenderTrianglesGPU(vertices, tex);
    }
}

void Renderer::RenderTrianglesCPU(const std::vector<VERTEX>& vertices, const Texture* tex) {
    _rendererCPU->RenderTriangles(vertices, tex);
}

void Renderer::RenderTrianglesCPU(const std::vector<std::vector<VERTEX>*>& vertices, const Texture* tex) {
    _rendererCPU->RenderTriangles(vertices, tex);
}

void Renderer::RenderTrianglesGPU(const std::vector<VERTEX>& vertices, const Texture* tex) {
    _rendererGPU->RenderTriangles(vertices, tex);
}

void Renderer::RenderTrianglesGPU(const std::vector<std::vector<VERTEX>*>& vertices, const Texture* tex) {
    _rendererGPU->RenderTriangles(vertices, tex);
}

// =============================================================================
// TRIANGLE RASTERIZATION - TEXTURED
// =============================================================================

void Renderer::DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
    _rendererCPU->DrawTriangleTextured(vert1, vert2, vert3, tex);
}

// =============================================================================
// TRIANGLE RASTERIZATION - WIREFRAME (PIXEL BUFFER)
// =============================================================================

void Renderer::DrawTriangleWireframePxBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const WCHAR pixel_type, const unsigned short col) {
    // RENDERING LINES BETWEEN VERTICES
    DrawLinePxBuff((int) vert1.X(), (int) vert1.Y(), (int) vert2.X(), (int) vert2.Y(), pixel_type, col);
    DrawLinePxBuff((int) vert2.X(), (int) vert2.Y(), (int) vert3.X(), (int) vert3.Y(), pixel_type, col);
    DrawLinePxBuff((int) vert3.X(), (int) vert3.Y(), (int) vert1.X(), (int) vert1.Y(), pixel_type, col);
}

// =============================================================================
// TRIANGLE RASTERIZATION - WIREFRAME (COLOR BUFFER)
// =============================================================================

void Renderer::DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::ivec4& col) {
    // RENDERING LINES BETWEEN VERTICES
    DrawLineColBuff((int) vert1.X(), (int) vert1.Y(), (int) vert2.X(), (int) vert2.Y(), col);
    DrawLineColBuff((int) vert2.X(), (int) vert2.Y(), (int) vert3.X(), (int) vert3.Y(), col);
    DrawLineColBuff((int) vert3.X(), (int) vert3.Y(), (int) vert1.X(), (int) vert1.Y(), col);
}

void Renderer::DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::ivec3& col) {
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
    DrawLinePxBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, _charRamp[8], col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, _charRamp[8], col);
    DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, _charRamp[8], col);
    DrawLinePxBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, _charRamp[8], col);
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
    if (_cpu_only) {
        _rendererCPU->BeginColBuffFrame();
    }
    else {
        _rendererGPU->BeginColBuffFrame();
    }
    
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
    
    size_t i = 0;
    for (; i < unrolledEnd; i += unrollFactor) {
        const auto& color0 = _color_buffer[i];
        const auto& color1 = _color_buffer[i + 1];
        const auto& color2 = _color_buffer[i + 2];
        const auto& color3 = _color_buffer[i + 3];
        

        const int index0 = (color0.r * _rgbLUTDepth * _rgbLUTDepth) + (color0.g * _rgbLUTDepth) + color0.b;
        const int index1 = (color1.r * _rgbLUTDepth * _rgbLUTDepth) + (color1.g * _rgbLUTDepth) + color1.b;
        const int index2 = (color2.r * _rgbLUTDepth * _rgbLUTDepth) + (color2.g * _rgbLUTDepth) + color2.b;
        const int index3 = (color3.r * _rgbLUTDepth * _rgbLUTDepth) + (color3.g * _rgbLUTDepth) + color3.b;
        
        pixelBuffer[i] = _colorLUT[index0];
        pixelBuffer[i + 1] = _colorLUT[index1];
        pixelBuffer[i + 2] = _colorLUT[index2];
        pixelBuffer[i + 3] = _colorLUT[index3];
    }
    
    for (; i < bufferSize; ++i) {
        const auto& color = _color_buffer[i];
        const int index = (color.r * _rgbLUTDepth * _rgbLUTDepth) + (color.g * _rgbLUTDepth) + color.b;
        pixelBuffer[i] = _colorLUT[index];
    }
}

// =============================================================================
// COLOR LOOKUP TABLE (LUT)
// =============================================================================

void Renderer::PrecomputeColorLUT() {
    Palette& palette = Screen::GetInst().GetPalette();
    if (_colorLUTComputed) return;
    
    const float invPaletteDepth = 1.0f / 15.0f;
    
    // Precompute all possible RGB combinations
    for (int r = 0; r < static_cast<int>(_rgbLUTDepth); ++r) {
        for (int g = 0; g < static_cast<int>(_rgbLUTDepth); ++g) {
            for (int b = 0; b < static_cast<int>(_rgbLUTDepth); ++b) {
                // Convert discrete RGB to normalized [0,1] range for contrast adjustment
                glm::vec3 rgb(
                    r*invPaletteDepth,
                    g*invPaletteDepth,
                    b*invPaletteDepth
                );

                // Apply contrast adjustment
                rgb = (rgb - 0.5f) * _contrast + 0.5f;
                rgb = glm::clamp(rgb, 0.0f, 1.0f);

                // Find best match using original algorithm
                float minError = FLT_MAX;
                int bestFgIndex = 0, bestBgIndex = 0, bestCharIndex = 0;

                for (int fgIdx = 0; fgIdx < static_cast<int>(palette.COLOR_COUNT); ++fgIdx) {
                    for (int bgIdx = 0; bgIdx < static_cast<int>(palette.COLOR_COUNT); ++bgIdx) {
                        for (int charIdx = 0; charIdx < static_cast<int>(_charRamp.size()); ++charIdx) {
                            float coverage = _charCoverage[charIdx];
                            
                            // Convert palette colors from 0-15 to 0.0-1.0 for comparison
                            glm::vec3 fgColor = glm::vec3(palette.GetRGB(fgIdx)) * invPaletteDepth;
                            glm::vec3 bgColor = glm::vec3(palette.GetRGB(bgIdx)) * invPaletteDepth;
                            
                            glm::vec3 simulatedColor = coverage * fgColor + (1.0f - coverage) * bgColor;
                            glm::vec3 diff = rgb - simulatedColor;
                            float error = glm::dot(diff, diff);

                            if (error < minError) {
                                minError = error;
                                bestFgIndex = fgIdx;
                                bestBgIndex = bgIdx;
                                bestCharIndex = charIdx;
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
                    std::wstringstream ss;
                    ss << L"PrecomputeColorLUT: Invalid color at index " << index 
                       << L" RGB(" << r << L"," << g << L"," << b << L")"
                       << L" FG:0x" << std::hex << fgColor 
                       << L" BG:0x" << bgColor
                       << L" fgIdx:" << std::dec << bestFgIndex 
                       << L" bgIdx:" << bestBgIndex;
                    Logger::Error(ss.str());
                    
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

    // Pre-computed _rgbLUTDepth squared for faster indexing
    const int index = (rgb.r * _rgbLUTDepth * _rgbLUTDepth) + (rgb.g * _rgbLUTDepth) + rgb.b;

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
    _background_col = glm::clamp(col, glm::ivec3(0), glm::ivec3(15));
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
// RENDER SETTINGS - CONTRAST
// =============================================================================

void Renderer::SetContrast(const float contrast) {
    _contrast = std::min(5.0f, std::max(0.0f, contrast));
    _colorLUTComputed = false; // Mark LUT for recomputation

    if (!_cpu_only) {
        _rendererGPU->SetContrastUniform(contrast);
    }
}

float Renderer::GetContrast() const {
    return _contrast;
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
        std::wstringstream ss;
        ss << L"Color " << colorIdx << L" RGB: ("
           << palette.GetRGB(colorIdx).r << L", "
           << palette.GetRGB(colorIdx).g << L", "
           << palette.GetRGB(colorIdx).b << L") "
           << "FG: " << palette.GetFgColor(colorIdx) << L" | BG: " << palette.GetBgColor(colorIdx);
        Logger::Debug(ss.str());

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
    if (_cpu_only) {
        // no action needed for CPU renderer
    }
    else {
        _rendererGPU->EndColBuffFrame();
    }

    OverwritePxBuffWithColBuff();
}