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
    VertStructs::PosWUVInvW HomogenousPlaneIntersect(const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert1, int component, bool Near);
    void ClipTriAgainstPlane(const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, std::vector<VertStructs::PosWUVInvW>& output, int component, bool Near);
    void ClippingHelper(std::vector<VertStructs::PosWUVInvW>& vertices, std::vector<VertStructs::PosWUVInvW>& clipped);
    void ClippingHelperThreaded(std::vector<VertStructs::PosWUVInvW>& vertices, std::vector<VertStructs::PosWUVInvW>& clipped);
    void ClippingHelperSingleThreaded(std::vector<VertStructs::PosWUVInvW>& vertices, std::vector<VertStructs::PosWUVInvW>& clipped);

    // =========================================================================
    // Backface Culling Stage
    // =========================================================================
    bool BackFaceCull(const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, bool CCW);
    void BackFaceCullHelper(std::vector<VertStructs::PosWUVInvW>& vertices);

    // =========================================================================
    // Transformation Stage
    // =========================================================================
    void PerspectiveAndViewportTransform(std::vector<VertStructs::PosWUVInvW>& raster_triangles);

    // =========================================================================
    // Tile-Based Rasterization
    // =========================================================================
    void DrawTiles(const std::vector<VertStructs::PosWUVInvW>& raster_triangles, const Texture* tex);
    void DrawTileTextured(const Tile& tile, const std::vector<VertStructs::PosWUVInvW>& raster_triangles, const Texture* tex);
    void DrawTileWireframe(const Tile& tile, const std::vector<VertStructs::PosWUVInvW>& raster_triangles);

    // =========================================================================
    // Triangle Rasterization Functions
    // =========================================================================
    void DrawTriangleTexturedImpl(const VertStructs::PosWUVInvW& v1, const VertStructs::PosWUVInvW& v2, const VertStructs::PosWUVInvW& v3, const Texture* tex, int minX, int maxX, int minY, int maxY);
    void DrawTriangleTexturedPartial(const Tile& tile, const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, const Texture* tex);
    void DrawTriangleTextured(const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, const Texture* tex);
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
    void RenderTriangles(const std::vector<std::vector<std::byte>*>& vertices, const VertFormat& format, const Texture* tex);
    void RenderTriangles(const std::vector<std::byte>& vertices, const VertFormat& format, const Texture* tex);

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

} // namespace ASCIIgL