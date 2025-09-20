#pragma once

#include <vector>

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
    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // general rendering settings
    static inline bool _wireframe = false;
    static inline bool _backface_culling = true;
    static inline bool _ccw = false;
    static inline bool _grayscale = false;
    static inline float _contrast = 1.2f;
    
    // antialaising
    static inline int _antialiasing_samples = 4;
    static inline bool _antialiasing = true;
    static inline std::vector<std::pair<float, float>> _subpixel_offsets;
    static std::vector<std::pair<float, float>> GenerateSubPixelOffsets(int sampleCount); // this is for antialiasing samples
    static std::vector<std::pair<float, float>> GetSubpixelOffsets();

    // Pre-allocated buffers for performance (avoid per-frame allocations)
    static inline std::vector<VERTEX> _vertexBuffer;
    static inline std::vector<VERTEX> _clippedBuffer;
    static inline std::vector<VERTEX> _rasterBuffer;
    static inline std::vector<Tile> _tileBuffer;
    static inline bool _tilesInitialized = false;

    // clipping stage
    static VERTEX HomogenousPlaneIntersect(const VERTEX& vert2, const VERTEX& vert1, const int component, const bool Near);
    static std::vector<VERTEX> Clipping(const std::vector<VERTEX>& vertices, const int component, const bool Near);
    static void ClippingHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);

    // backface culling
    static bool BackFaceCull(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const bool CCW);
    static void BackFaceCullHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& raster_triangles);

    // viewport and perspective transform
    static void PerspectiveDivision(VERTEX& clipCoord);
    static void ViewPortTransform(VERTEX& vertice);

    // Tile setup
    static void InitializeTiles(); // One-time tile setup
    static void ClearTileTriangleLists(); // Clear triangle lists but keep tile structure
    static void BinTrianglesToTiles(const std::vector<VERTEX>& raster_triangles);
    static bool DoesTileEncapsulate(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3);
    static void InvalidateTiles();
    
    static void DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    static void DrawTriangleWireframePartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const WCHAR pixel_type, const COLOR col);

    static void DrawTriangleTexturedAntialiased(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    static void DrawTriangleTexturedPartialAntialiased(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    
    static void DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    static void DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles);

    // Glyphs
    static inline const std::array<wchar_t, 9> _charRamp = {
        L' ',  // space (darkest)
        L'-',
        176,   // ░ (Light shade - DOS char 176)
        L'o',
        L'@',
        177,   // ▒ (Medium shade - DOS char 177)
        L'#',
        178,   // ▓ (Dark shade - DOS char 178)  
        219,   // █ (Full block - DOS char 219)
    };

    static inline const std::array<float, 9> _charCoverage = {
        0.00f,  // ' ' (space)
        0.18f,  // '-'
        0.28f,  // ░ (176)
        0.32f,  // 'o'
        0.60f,  // '@'
        0.68f,  // ▒ (177)
        0.76f,  // '#'
        0.86f,  // ▓ (178)
        1.00f   // █ (219)
    };

    // Colored lookup
    static CHAR_INFO GetCharInfoRGB(const glm::vec3& rgb);

    // Precompute a full LUT for all combinations of fg, bg, and char for super fast rendering
    // 16*16*7 is for all of the characters that need fractional rendering, these need to blend the background and foreground
    // + 2*16 is for the space and full block characters which are just solid colors
    static void PrecomputeColorLUT();
    static inline bool _colorLUTComputed = false;
    static inline std::array<CHAR_INFO, Palette::COLOR_COUNT*Palette::COLOR_COUNT*7 + 2*Palette::COLOR_COUNT> _colorLUT;


public:
    static void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh);
    static void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    static void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const Camera3D& camera);
    static void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    static void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4& model, const Camera3D& camera);
    static void Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer);
    static void Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer);
    static void DrawScreenBorder(const COLOR col);
    
    static void RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex);

    // NOT THREAD SAFE, NOT TILE BASED
    static void DrawLine(int x1, int y1, int x2, int y2, WCHAR pixel_type, const COLOR col);
    static void DrawTriangleWireframe(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, WCHAR pixel_type, const COLOR col);
    static void DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    static glm::mat4 CalcModelMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size);
    static glm::mat4 CalcModelMatrix(const glm::vec3& position, const float rotation, const glm::vec3& size);

    // Antialiasing configuration
    static void SetAntialiasingsamples(const int samples);
    static int GetAntialiasingsamples();

    // Rendering configuration getters and setters
    static void SetWireframe(const bool wireframe);
    static bool GetWireframe();
    
    static void SetBackfaceCulling(const bool backfaceCulling);
    static bool GetBackfaceCulling();

    static void SetCCW(const bool ccw);
    static bool GetCCW();

    static void SetAntialiasing(const bool antialiasing);
    static bool GetAntialiasing();

    static void SetContrast(const float contrast);
    static float GetContrast();

    static void SetGrayscale(const bool grayscale);
    static bool GetGrayscale();

    static void TestRender();
};