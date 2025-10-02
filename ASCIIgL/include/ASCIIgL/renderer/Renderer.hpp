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

struct Tile {
    glm::vec2 position;
    glm::vec2 size;
    std::vector<int> tri_indices_encapsulated;
    std::vector<int> tri_indices_partial;
};

class Renderer
{
private:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // general rendering settings
    bool _wireframe = false;
    bool _backface_culling = true;
    bool _ccw = false;
    float _contrast = 1.2f;
    glm::vec3 _background_col = glm::vec3(0.0f, 0.0f, 0.0f);

    // buffers for rendering
    std::vector<glm::vec4> _color_buffer;
    std::vector<float> _depth_buffer;

    void PlotColor(int x, int y, const glm::vec4& color, float depth);
    void PlotColorBlend(int x, int y, const glm::vec4& color, float depth);

    // antialiasing
    int _antialiasing_samples = 4;
    bool _antialiasing = false;
    std::vector<std::pair<float, float>> _subpixel_offsets;
    void GenerateSubPixelOffsets(int sampleCount);
    const std::vector<std::pair<float, float>>& GetSubpixelOffsets();

    // Pre-allocated buffers for performance (avoid per-frame allocations)
    std::vector<VERTEX> _vertexBuffer;
    std::vector<VERTEX> _clippedBuffer;
    std::vector<Tile> _tileBuffer;
    bool _tilesInitialized = false;

    // clipping stage
    VERTEX HomogenousPlaneIntersect(const VERTEX& vert2, const VERTEX& vert1, const int component, const bool Near);
    std::vector<VERTEX> Clipping(const std::vector<VERTEX>& vertices, const int component, const bool Near);
    void ClippingHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);

    // backface culling
    bool BackFaceCull(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const bool CCW);
    void BackFaceCullHelper(std::vector<VERTEX>& vertices);

    // viewport and perspective transform
    void PerspectiveDivision(VERTEX& clipCoord);
    void ViewPortTransform(VERTEX& vertice);
    void PerspectiveAndViewportTransformHelper(std::vector<VERTEX>& raster_triangles);

    // Tile setup
    void InitializeTiles();
    void ClearTileTriangleLists();
    void BinTrianglesToTiles(const std::vector<VERTEX>& raster_triangles);
    bool DoesTileEncapsulate(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3);
    void InvalidateTiles();

    void DrawTriangleWireframeColBuffPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col);
    void DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    void DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    void DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles);

    // Line drawing with tile clipping
    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col);
    void DrawClippedLineColBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, const glm::vec4& col);

    // Glyphs
    static constexpr std::array<wchar_t, 9> _charRamp = {
        L' ', L'-', 0x2591, L'o', L'@', 0x2592, L'#', 0x2593, 0x2588
    };

    static constexpr std::array<float, 9> _charCoverage = {
        0.00f, 0.18f, 0.28f, 0.32f, 0.60f, 0.70f, 0.76f, 0.86f, 1.00f
    };

    void PrecomputeColorLUT();
    bool _colorLUTComputed = false;
    static constexpr unsigned int _rgbLUTDepth = 16;
    std::array<CHAR_INFO, _rgbLUTDepth*_rgbLUTDepth*_rgbLUTDepth> _colorLUT;

public:
    static Renderer& GetInst() {
        static Renderer instance;
        return instance;
    }

    void Initialize();

    CHAR_INFO GetCharInfo(const glm::vec3& rgb);

    void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh);
    void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const Camera3D& camera);
    void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4& model, const Camera3D& camera);
    void Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer);
    void Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer);
    void DrawScreenBorderPxBuff(const unsigned short col);
    void DrawScreenBorderColBuff(const glm::vec3& col);

    void RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex);

    void DrawLinePxBuff(int x1, int y1, int x2, int y2, WCHAR pixel_type, const unsigned short col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::vec4& col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::vec3& col);

    void DrawTriangleWireframePxBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, WCHAR pixel_type, const unsigned short col);
    void DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col);
    void DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec3& col);

    void DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    glm::mat4 CalcModelMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size);
    glm::mat4 CalcModelMatrix(const glm::vec3& position, const float rotation, const glm::vec3& size);

    void PlotColor(int x, int y, const glm::vec3& color);
    void PlotColor(int x, int y, const glm::vec4& color);

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

    glm::vec3 GetBackgroundCol() const;
    void SetBackgroundCol(const glm::vec3& color);

    void OverwritePxBuffWithColBuff();
    void ClearBuffers();

    void TestRenderFont();
    void TestRenderColorDiscrete();
};