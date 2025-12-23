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

// Forward declaration
class Renderer;

class RendererCPU
{
    friend class Renderer;

private:
    // =========================================================================
    // Singleton Pattern (Non-copyable, Non-movable)
    // =========================================================================
    RendererCPU() = default;
    ~RendererCPU();
    RendererCPU(const RendererCPU&) = delete;
    RendererCPU& operator=(const RendererCPU&) = delete;

    // =========================================================================
    // Initialization State
    // =========================================================================
    void Initialize();
    bool _initialized = false;
    Renderer* _renderer = nullptr;
    VERTEX_SHADER_CPU _vertex_shader;

    // =========================================================================
    // Rendering Buffers
    // =========================================================================
    std::vector<float> _depth_buffer;
    std::vector<VERTEX> _vertexBuffer;
    std::vector<VERTEX> _clippedBuffer;

    // =========================================================================
    // Antialiasing
    // =========================================================================
    std::vector<std::pair<float, float>> _subpixel_offsets;
    
    void GenerateSubPixelOffsets(int sampleCount);
    const std::vector<std::pair<float, float>>& GetSubpixelOffsets();

    // =========================================================================
    // Diagnostics
    // =========================================================================
    unsigned int _triangles_inputted = 0;
    unsigned int _triangles_past_clipping = 0;
    unsigned int _triangles_past_backface_culling = 0;

    // =========================================================================
    // Rendering Pipeline
    // =========================================================================
    void RenderPipeline(const Texture* tex);

    // =========================================================================
    // Clipping Stage
    // =========================================================================
    VERTEX HomogenousPlaneIntersect(const VERTEX& vert2, const VERTEX& vert1, const int component, const bool Near);
    void ClipTriAgainstPlane(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, std::vector<VERTEX>& output, const int component, const bool Near);
    void ClippingHelper(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);
    void ClippingHelperThreaded(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);
    void ClippingHelperSingleThreaded(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped);

    // =========================================================================
    // Backface Culling Stage
    // =========================================================================
    bool BackFaceCull(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const bool CCW);
    void BackFaceCullHelper(std::vector<VERTEX>& vertices);

    // =========================================================================
    // Transformation Stage
    // =========================================================================
    void PerspectiveAndViewportTransform(std::vector<VERTEX>& raster_triangles);

    // =========================================================================
    // Tile-Based Rasterization
    // =========================================================================
    void DrawTiles(const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    void DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex);
    void DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles);

    // =========================================================================
    // Triangle Rasterization Functions
    // =========================================================================
    void DrawTriangleTexturedImpl(const VERTEX& v1, const VERTEX& v2, const VERTEX& v3, const Texture* tex, int minX, int maxX, int minY, int maxY);
    void DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);
    void DrawTriangleWireframeColBuffPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col);

    // =========================================================================
    // Pixel Plotting
    // =========================================================================
    void PlotColorBlend(int x, int y, const glm::ivec4& color, float depth);

    // =========================================================================
    // Frame Management
    // =========================================================================
    void BeginColBuffFrame();

    // =========================================================================
    // High-Level Drawing API - Meshes and Models
    // =========================================================================
    void DrawMesh(const Mesh* mesh);
    void DrawMesh(const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    
    void DrawModel(const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera);
    void DrawModel(const Model& ModelObj, const glm::mat4& model, const Camera3D& camera);

    // =========================================================================
    // High-Level Drawing API - 2D Quads (Friend-Accessible Only via Renderer)
    // =========================================================================
    void Draw2DQuadPixelSpace(const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer);
    void Draw2DQuadPercSpace(const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer);

    // ========================================================================
    // Mid-level API - Rendering Pipelines (Friend-Accessible Only via Renderer)
    // =========================================================================
    void RenderTriangles(const std::vector<VERTEX>& vertices, const Texture* tex);
    void RenderTriangles(const std::vector<std::vector<VERTEX>*>& vertices, const Texture* tex);

    // =========================================================================
    // Low-Level Drawing API - Primitives (Friend-Accessible Only via Renderer)
    // =========================================================================
    void DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex);

public:
    // =========================================================================
    // Singleton Access
    // =========================================================================
    static RendererCPU& GetInst() {
        static RendererCPU instance;
        return instance;
    }

    // =========================================================================
    // Lifecycle Management
    // =========================================================================
    bool IsInitialized() const;

    // =========================================================================
    // Vertex Shader Management
    // =========================================================================
    VERTEX_SHADER_CPU& GetVShader();
};