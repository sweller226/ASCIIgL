#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Model.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>

#include <ASCIIgL/renderer/VertexShader.hpp>
#include <ASCIIgL/renderer/Screen.hpp>

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
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    static inline int _antialiasing_samples = 4;
    static inline bool _wireframe = false;
    static inline bool _backface_culling = true;
    static inline bool _ccw = false;
    static inline bool _antialiasing = true;

    // Pre-allocated buffers for performance (avoid per-frame allocations)
    static inline std::vector<VERTEX> _vertexBuffer;
    static inline std::vector<VERTEX> _clippedBuffer;
    static inline std::vector<VERTEX> _rasterBuffer;
    static inline std::vector<Tile> _tileBuffer;
    static inline bool _tilesInitialized = false;

    static VERTEX HomogenousPlaneIntersect(const VERTEX& vert2, const VERTEX& vert1, const int component, const bool Near);
    static std::vector<VERTEX> Clipping(const std::vector<VERTEX>& vertices, const int component, const bool Near);
    static void PerspectiveDivision(VERTEX& clipCoord);
    static void ClippingHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);
    static bool BackFaceCull(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, bool CCW);
    static void ViewPortTransform(VERTEX& vertice);
    static void BackFaceCullHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& raster_triangles);

    static void InitializeTiles(); // One-time tile setup
    static void ClearTileTriangleLists(); // Clear triangle lists but keep tile structure
    static void BinTrianglesToTiles(const std::vector<VERTEX>& raster_triangles); // Optimized version
    static bool DoesTileEncapsulate(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3);
    
    static void DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    static void DrawTriangleTexturedAntialiased(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    static void DrawTriangleTexturedPartialAntialiased(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    static std::vector<std::pair<float, float>> GenerateSubPixelOffsets(int sampleCount);

    static void DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    // static void DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles);

    public:
    static Renderer& GetInstance() {
        static Renderer instance;
        return instance;
    }
    
    static void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh);
    static void DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3 position, const glm::vec2 rotation, const glm::vec3 size, const Camera3D& camera);
    static void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3 position, const glm::vec2 rotation, const glm::vec3 size, const Camera3D& camera);
    static void DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4 model, const Camera3D& camera);
    static void Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2 position, const glm::vec2 rotation, const glm::vec2 size, const Camera2D& camera, int layer);
    static void Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2 positionPerc, const glm::vec2 rotation, const glm::vec2 sizePerc, const Camera2D& camera, int layer);
    static void DrawScreenBorder(short col);
    
    static void RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex);

    // NOT THREAD SAFE, NOT TILE BASED
    static void DrawLine(int x1, int y1, int x2, int y2, CHAR pixel_type, short col);
    static void DrawTriangleWireFrame(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, CHAR pixel_type, short col);
    static void DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

    static glm::mat4 CalcModelMatrix(const glm::vec3 position, const glm::vec2 rotation, const glm::vec3 size);
    static float GrayScaleRGB(const glm::vec3 rgb);
    static CHAR_INFO GetColGlyphGreyScale(const float GreyScale);

    // Antialiasing configuration
    static void SetAntialiasingsamples(int samples);
    static int GetAntialiasingsamples();

    // Rendering configuration getters and setters
    static void SetWireframe(bool wireframe);
    static bool GetWireframe();
    
    static void SetBackfaceCulling(bool backfaceCulling);
    static bool GetBackfaceCulling();
    
    static void SetCCW(bool ccw);
    static bool GetCCW();
    
    static void SetAntialiasing(bool antialiasing);
    static bool GetAntialiasing();

    // Performance optimization - call when screen dimensions change
    static void InvalidateTiles(); // Forces tile re-initialization on next render
};