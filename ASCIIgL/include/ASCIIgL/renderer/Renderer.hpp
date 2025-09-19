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
#include <ASCIIgL/renderer/RenderEnums.hpp>

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
    static inline float _contrast = 1.5f;
    
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
    
    static void DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    static void DrawTriangleWireframePartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const CHAR pixel_type, const COLOR col);

    static void DrawTriangleTexturedAntialiased(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    static void DrawTriangleTexturedPartialAntialiased(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    
    static void DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    static void DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles);

    // Glyphs
    static inline constexpr unsigned int _max_char_variety = static_cast<unsigned int>(CHAR_VARIETY::EIGHT) + 1;
    static inline CHAR_VARIETY _char_variety = CHAR_VARIETY::FOUR;
    static inline unsigned int _char_options = static_cast<unsigned int>(_char_variety) + 1;

    static constexpr std::array<PX_TYPE, _max_char_variety> _glyphPixelsEight = {
        PX_TYPE::PX_NONE, PX_TYPE::PX_ONE_EIGHTH, PX_TYPE::PX_QUARTER,
        PX_TYPE::PX_THREE_EIGHTHS, PX_TYPE::PX_HALF, PX_TYPE::PX_FIVE_EIGHTHS,
        PX_TYPE::PX_THREE_QUARTERS, PX_TYPE::PX_SEVEN_EIGHTHS, PX_TYPE::PX_FULL
    };
    static constexpr std::array<PX_TYPE, _max_char_variety> _glyphPixelsFour = {
        PX_TYPE::PX_NONE, PX_TYPE::PX_QUARTER, PX_TYPE::PX_HALF, PX_TYPE::PX_THREE_QUARTERS, PX_TYPE::PX_FULL
    };
    static inline const PX_TYPE* _glyphPixels = _glyphPixelsFour.data();

    // Grayscale lookup
    static float GrayScaleRGB(const glm::vec3& rgb);
    static CHAR_INFO GetCharInfoGreyScale(const float intensity);

    static inline const COLOR greyScaleCodes[4] = {
        COLOR::FG_BLACK, COLOR::FG_DARK_GREY, COLOR::FG_GREY, COLOR::FG_WHITE,
    };

    // Colored lookup
    static CHAR_INFO GetCharInfoRGB(const glm::vec3& rgb);
    static CHAR_INFO GetCharInfoRGB(const glm::vec3& rgb, const float intensity);

    // Pre-compiled console color codes for fast lookup
    static inline const std::array<glm::vec3, 16> consoleColors = {
        glm::vec3(0.0f, 0.0f, 0.0f),           // Black
        glm::vec3(0.0f, 0.0f, 0.545f),         // Dark Blue
        glm::vec3(0.0f, 0.5f, 0.0f),         // Dark Green
        glm::vec3(0.0f, 0.545f, 0.545f),       // Dark Cyan
        glm::vec3(0.545f, 0.0f, 0.0f),         // Dark Red
        glm::vec3(0.545f, 0.0f, 0.545f),       // Dark Magenta
        glm::vec3(0.647f, 0.165f, 0.165f),     // Brown (Dark Yellow)
        glm::vec3(0.753f, 0.753f, 0.753f),     // Light Gray
        glm::vec3(0.412f, 0.412f, 0.412f),     // Dark Gray
        glm::vec3(0.0f, 0.0f, 0.9f),           // Blue
        glm::vec3(0.0f, 0.8f, 0.0f),           // Green
        glm::vec3(0.0f, 0.9f, 0.9f),           // Cyan
        glm::vec3(0.9f, 0.0f, 0.0f),           // Red
        glm::vec3(0.9f, 0.0f, 0.9f),           // Magenta
        glm::vec3(0.9f, 0.9f, 0.0f),           // Yellow
        glm::vec3(0.9f, 0.9f, 0.9f)            // White
    };

    static constexpr std::array<COLOR, 16> colorCodes = {
        COLOR::FG_BLACK, COLOR::FG_DARK_BLUE, COLOR::FG_DARK_GREEN, COLOR::FG_DARK_CYAN,
        COLOR::FG_DARK_RED, COLOR::FG_DARK_MAGENTA, COLOR::FG_DARK_YELLOW, COLOR::FG_GREY,
        COLOR::FG_DARK_GREY, COLOR::FG_BLUE, COLOR::FG_GREEN, COLOR::FG_CYAN,
        COLOR::FG_RED, COLOR::FG_MAGENTA, COLOR::FG_YELLOW, COLOR::FG_WHITE
    };


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
    static void DrawLine(int x1, int y1, int x2, int y2, CHAR pixel_type, const COLOR col);
    static void DrawTriangleWireframe(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, CHAR pixel_type, const COLOR col);
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

    static void SetCharVariety(const CHAR_VARIETY variety);
    static CHAR_VARIETY GetCharVariety();

    // Performance optimization - call when screen dimensions change
    static void InvalidateTiles(); // Forces tile re-initialization on next render
};