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

#include <ASCIIgL/renderer/VertexShader.hpp>
#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/TileManager.hpp>

class Renderer
{
private:
    // =========================================================================
    // Singleton and Construction
    // =========================================================================
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // =========================================================================
    // Rendering Settings
    // =========================================================================
    bool _wireframe = false;
    bool _backface_culling = true;
    bool _ccw = false;
    float _contrast = 1.0f;
    glm::ivec3 _background_col = glm::ivec3(0, 0, 0);
    bool _cpu_only = false;

    // =========================================================================
    // Buffers
    // =========================================================================
    std::vector<glm::ivec4> _color_buffer;
    std::vector<float> _depth_buffer;
    std::vector<VERTEX> _vertexBuffer;
    std::vector<VERTEX> _clippedBuffer;

    // =========================================================================
    // Antialiasing
    // =========================================================================
    int _antialiasing_samples = 4;
    bool _antialiasing = false;
    std::vector<std::pair<float, float>> _subpixel_offsets;
    void GenerateSubPixelOffsets(int sampleCount);
    const std::vector<std::pair<float, float>>& GetSubpixelOffsets();

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
    unsigned int _triangles_inputted = 0;
    unsigned int _triangles_past_clipping = 0;
    unsigned int _triangles_past_backface_culling = 0;

    // =========================================================================
    // Core Private Rendering Functions
    // =========================================================================
    void RenderTrianglesGPU(const VERTEX_SHADER& VSHADER, const Texture* tex);
    void RenderTrianglesCPU(const VERTEX_SHADER& VSHADER, const Texture* tex);

    // =========================================================================
    // Clipping
    // =========================================================================
    VERTEX HomogenousPlaneIntersect(const VERTEX& vert2, const VERTEX& vert1, const int component, const bool Near);
    void ClipTriAgainstPlane(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, std::vector<VERTEX>& output, const int component, const bool Near);
    void ClippingHelperThreaded(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);
    void ClippingHelperSingleThreaded(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);
    void ClippingHelper(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);

    // =========================================================================
    // Backface Culling
    // =========================================================================
    bool BackFaceCull(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const bool CCW);
    void BackFaceCullHelper(std::vector<VERTEX>& vertices);

    // =========================================================================
    void PerspectiveAndViewportTransform(std::vector<VERTEX>& raster_triangles);

    // =========================================================================
    // Rasterization and Drawing Helpers
    // =========================================================================

    void DrawTriangleWireframeColBuffPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col);
    void DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    void DrawTriangleTexturedImpl(const VERTEX& v1, const VERTEX& v2, const VERTEX& v3, const Texture* tex, int minX, int maxX, int minY, int maxY);

    void DrawTiles(const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    void DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    void DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles);

    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col);
    void DrawClippedLineColBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, const glm::vec4& col);
    void PlotColor(int x, int y, const glm::ivec4& color, float depth);
    void PlotColorBlend(int x, int y, const glm::ivec4& color, float depth);

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
    void Initialize();
    void RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex);
    void RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<std::vector<VERTEX>*>& vertices, const Texture* tex);

    // =========================================================================
    // Drawing API
    // =========================================================================
    CHAR_INFO GetCharInfo(const glm::ivec3& rgb);

    void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh);
    void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4& model, const Camera3D& camera);
    void Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer);
    void Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer);
    void DrawScreenBorderPxBuff(const unsigned short col);
    void DrawScreenBorderColBuff(const glm::vec3& col);

    void DrawLinePxBuff(int x1, int y1, int x2, int y2, WCHAR pixel_type, const unsigned short col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::vec4& col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::vec3& col);

    void DrawTriangleWireframePxBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, WCHAR pixel_type, const unsigned short col);
    void DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col);
    void DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec3& col);

    void DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    // =========================================================================
    // Matrix and Utility
    // =========================================================================
    glm::mat4 CalcModelMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size);
    glm::mat4 CalcModelMatrix(const glm::vec3& position, const float rotation, const glm::vec3& size);

    void PlotColor(int x, int y, const glm::ivec3& color);
    void PlotColor(int x, int y, const glm::ivec4& color);

    // =========================================================================
    // Settings API
    // =========================================================================
    void SetAntialiasingsamples(const int samples);
    int GetAntialiasingsamples() const;

    void SetWireframe(const bool wireframe);
    bool GetWireframe() const;

    void SetBackfaceCulling(const bool backfaceCulling);
    bool GetBackfaceCulling() const;

    void SetCCW(const bool ccw);
    bool GetCCW() const;

    void SetAntialiasing(const bool antialiasing);
    bool GetAntialiasing() const;

    void SetContrast(const float contrast);
    float GetContrast() const;

    glm::ivec3 GetBackgroundCol() const;
    void SetBackgroundCol(const glm::ivec3& color);

    void SetDiagnosticsEnabled(const bool enabled);
    bool GetDiagnosticsEnabled() const;

    void SetCpuOnly(const bool cpu_only);
    bool GetCpuOnly() const;

    // =========================================================================
    // Buffer and Diagnostics
    // =========================================================================
    void OverwritePxBuffWithColBuff();
    void ClearBuffers();
    void TestRenderFont();
    void TestRenderColorDiscrete();
    void TestRenderColorContinuous();
    const std::vector<glm::ivec4>& GetColorBuffer() const;
};