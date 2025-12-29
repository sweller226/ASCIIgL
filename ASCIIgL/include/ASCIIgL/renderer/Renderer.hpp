#pragma once

#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Model.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>

#include <ASCIIgL/renderer/VertexShaderCPU.hpp>
#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/TileManager.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

class RendererCPU;
class RendererGPU;

class Renderer
{
    friend class RendererCPU;
    friend class RendererGPU;

private:
    bool _initialized = false;

    // =========================================================================
    // Singleton and Construction
    // =========================================================================
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    
    RendererCPU* _rendererCPU = nullptr;  // Pointer to CPU renderer
    RendererGPU* _rendererGPU = nullptr;  // Pointer to GPU renderer

    // =========================================================================
    // Rendering Settings
    // =========================================================================
    bool _wireframe = false;
    bool _backface_culling = true;
    bool _ccw = false;

    glm::ivec3 _background_col = glm::ivec3(0, 0, 0);
    bool _cpu_only = false;

    // =========================================================================
    // Buffers
    // =========================================================================
    std::vector<glm::ivec4> _color_buffer;

    // =========================================================================
    // Antialiasing
    // =========================================================================
    int _antialiasing_samples = 4;
    bool _antialiasing = false;

    // =========================================================================
    // Glyphs and Color LUT
    // =========================================================================
    static constexpr std::array<wchar_t, 9> _charRamp = {
        L' ', L'-', L':', L'o', L'O', L'A', L'E', L'0', L'B'
    };
    
    static constexpr std::array<float, 9> _charCoverage = {
        0.00f, 0.12f, 0.20f, 0.35f, 0.50f, 0.65f, 0.75f, 0.85f, 0.90f
    };

    void PrecomputeColorLUT();
    bool _colorLUTComputed = false;
    static constexpr unsigned int _rgbLUTDepth = 16;
    std::array<CHAR_INFO, _rgbLUTDepth*_rgbLUTDepth*_rgbLUTDepth> _colorLUT;

    // =========================================================================
    // Diagnostics
    // =========================================================================
    void ResetDiagnostics();
    void LogDiagnostics() const;
    bool _diagnostics_enabled = false;

    // =========================================================================
    // Helper Drawing Functions
    // =========================================================================
    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col);
    void DrawClippedLineColBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, const glm::ivec4& col);
public:
    // =========================================================================
    // Singleton Access
    // =========================================================================
    static Renderer& GetInst() {
        static Renderer instance;
        return instance;
    }

    // =========================================================================
    // Initialization and Core API
    // =========================================================================
    void Initialize(bool antialiasing = false, int antialiasing_samples = 4, bool cpu_only = false);
    bool IsInitialized() const;

    // =========================================================================
    // Drawing API
    // =========================================================================
    void DrawMesh(const Mesh* mesh);
    void DrawMesh(const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawModel(const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawModel(const Model& ModelObj, const glm::mat4& model, const Camera3D& camera);
    void Draw2DQuadPixelSpace(const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer);
    void Draw2DQuadPercSpace(const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer);
    
    // =========================================================================
    // Low-Level Drawing API - Primitives, No pipeline involved
    // =========================================================================
    void DrawLinePxBuff(int x1, int y1, int x2, int y2, WCHAR pixel_type, const unsigned short col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::ivec4& col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::ivec3& col);

    void DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, WCHAR pixel_type, const unsigned short col);
    void DrawTriangleWireframeColBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::ivec4& col);
    void DrawTriangleWireframeColBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::ivec3& col);

    void DrawScreenBorderPxBuff(const unsigned short col);
    void DrawScreenBorderColBuff(const glm::vec3& col);

    void PlotColor(int x, int y, const glm::ivec3& color);
    void PlotColor(int x, int y, const glm::ivec4& color);
    void PlotColorBlend(int x, int y, const glm::ivec4& color);

    // =========================================================================
    // Utility
    // =========================================================================
    CHAR_INFO GetCharInfo(const glm::ivec3& rgb);

    // =========================================================================
    // Settings API
    // =========================================================================
    int GetAntialiasingsamples() const;

    void SetWireframe(const bool wireframe);
    bool GetWireframe() const;

    void SetBackfaceCulling(const bool backfaceCulling);
    bool GetBackfaceCulling() const;

    void SetCCW(const bool ccw);
    bool GetCCW() const;

    bool GetAntialiasing() const;

    glm::ivec3 GetBackgroundCol() const;
    void SetBackgroundCol(const glm::ivec3& color);

    void SetDiagnosticsEnabled(const bool enabled);
    bool GetDiagnosticsEnabled() const;
    
    bool GetCpuOnly() const;
    
    // =========================================================================
    // Buffer and Diagnostics
    // =========================================================================
    void OverwritePxBuffWithColBuff();
    void BeginColBuffFrame();
    void EndColBuffFrame();
    void TestRenderFont();
    void TestRenderColorDiscrete();
    void TestRenderColorContinuous();
    std::vector<glm::ivec4>& GetColorBuffer();
};

} // namespace ASCIIgL