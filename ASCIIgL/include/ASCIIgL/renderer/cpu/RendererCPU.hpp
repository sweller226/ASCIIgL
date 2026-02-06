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

#include <ASCIIgL/renderer/screen/Screen.hpp>

#include <ASCIIgL/renderer/cpu/VertexShaderCPU.hpp>
#include <ASCIIgL/renderer/cpu/TileManager.hpp>

#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

using V = VertStructs::PosWUVInvW;

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
    std::vector<VertStructs::PosWUVInvW> _vertexBuffer;
    std::vector<VertStructs::PosWUVInvW> _clippedBuffer;

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

    V HomogenousPlaneIntersect(const V& vert2, const V& vert1, int component, bool Near);
    void ClipTriAgainstPlane(const V& vert1, const V& vert2, const V& vert3, std::vector<V>& output, int component, bool Near);
    void ClippingHelper(std::vector<V>& vertices, std::vector<V>& clipped);
    void ClippingHelperThreaded(std::vector<V>& vertices, std::vector<V>& clipped);
    void ClippingHelperSingleThreaded(std::vector<V>& vertices, std::vector<V>& clipped);

    // =========================================================================
    // Backface Culling Stage
    // =========================================================================
    bool BackFaceCull(const V& vert1, const V& vert2, const V& vert3, bool CCW);
    void BackFaceCullHelper(std::vector<V>& vertices);

    // =========================================================================
    // Transformation Stage
    // =========================================================================
    void PerspectiveAndViewportTransform(std::vector<V>& raster_triangles);

    // =========================================================================
    // Tile-Based Rasterization
    // =========================================================================
    void DrawTiles(const std::vector<V>& raster_triangles, const Texture* tex);
    void DrawTileTextured(const Tile& tile, const std::vector<V>& raster_triangles, const Texture* tex);
    void DrawTileWireframe(const Tile& tile, const std::vector<V>& raster_triangles);

    // =========================================================================
    // Triangle Rasterization Functions
    // =========================================================================
    void DrawTriangleTexturedImpl(const V& v1, const V& v2, const V& v3, const Texture* tex, int minX, int maxX, int minY, int maxY);
    void DrawTriangleTexturedPartial(const Tile& tile, const V& vert1, const V& vert2, const V& vert3, const Texture* tex);
    void DrawTriangleTextured(const V& vert1, const V& vert2, const V& vert3, const Texture* tex);
    void DrawTriangleWireframeColBuffPartial(const Tile& tile, const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::vec4& col);

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
    void DrawModel(const Model& ModelObj);

    // ========================================================================
    // Mid-level API - Rendering Pipelines (Friend-Accessible Only via Renderer)
    // =========================================================================
    void RenderTriangles(const std::vector<std::vector<std::byte>*>& vertices, const VertFormat& format, const Texture* tex);
    void RenderTriangles(const std::vector<std::byte>& vertices, const VertFormat& format, const Texture* tex);

public:
    // =========================================================================
    // High-Level Drawing API - CPU only api
    // =========================================================================
    void Draw2DQuad(const Texture& tex);

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

} // namespace ASCIIgL