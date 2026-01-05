#include <ASCIIgL/renderer/cpu/RendererCPU.hpp>

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

#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/cpu/TileManager.hpp>

namespace ASCIIgL {

RendererCPU::~RendererCPU() {
    _depth_buffer.clear();
    _vertexBuffer.clear();
    _clippedBuffer.clear();
}

void RendererCPU::Initialize() {
    if (!_initialized) {
        Logger::Info("Initializing RendererCPU...");
    } else {
        Logger::Warning("RendererCPU is already initialized!");
        return;
    }

    if (!Screen::GetInst().IsInitialized()) {
        Logger::Error("Renderer: Screen must be initialized before creating Renderer.");
        throw std::runtime_error("Renderer: Screen must be initialized before creating Renderer.");
    }

    if (!_renderer) { // intialization should only be called from renderer, but just in case
        _renderer = &Renderer::GetInst();
    }

    auto& screen = Screen::GetInst();
    _depth_buffer.resize(screen.GetWidth() * screen.GetHeight());

    _vertexBuffer.reserve(100000);  // Enough for ~33k triangles
    _clippedBuffer.reserve(200000); // Clipping can double vertices

    _initialized = true;
}

VertStructs::PosWUVInvW RendererCPU::HomogenousPlaneIntersect(const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert1, const int component, const bool Near) {
    // Compute difference directly
    float i0 = vert1.data[component];
    float w0 = vert1.data[3];
    float vi = vert1.data[component] - vert2.data[component];
    float vw = vert1.data[3] - vert2.data[3];

    float denom;
    if (Near)
        denom = vw + vi;
    else
        denom = vi - vw;

    // Safety for divide by zero
    if (fabsf(denom) < 1e-7f) {
        Logger::Debug("HomogenousPlaneIntersect: Divide by zero detected, returning vert1.");
        return vert1;
    }

    float t;
    if (Near)
        t = (i0 + w0) / denom;
    else
        t = (i0 - w0) / denom;

    VertStructs::PosWUVInvW newVert;
    for (int i = 0; i < 7; ++i)
        newVert.data[i] = glm::mix(vert1.data[i], vert2.data[i], t);

    return newVert;
}

// =============================================================================
// PUBLIC HIGH-LEVEL DRAWING FUNCTIONS
// =============================================================================

void RendererCPU::DrawMesh(const Mesh* mesh) {
    RenderTriangles(mesh->GetVertices(), mesh->GetVertFormat(), mesh->GetTexture());
}


void RendererCPU::DrawModel(const Model& ModelObj) {
    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(ModelObj.meshes[i]);
    }
}

void RendererCPU::Draw2DQuad(const Texture& tex) {
    static const VertStructs::PosWUVInvW vertexDataCCW[] = {
        {{ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}}, // bottom-left
        {{ -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f}}, // top-left
        {{  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}}, // top-right
        {{  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}}, // top-right
        {{  1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f}}, // bottom-right
        {{ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}}, // bottom-left
    };

    static const VertStructs::PosWUVInvW vertexDataCW[] = {
        {{ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}}, // bottom-left
        {{  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}}, // top-right
        {{ -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f}}, // top-left

        {{ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}}, // bottom-left
        {{  1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f}}, // bottom-right
        {{  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}}, // top-right
    };

    const VertStructs::PosWUVInvW* vertexData = _renderer->_ccw ? vertexDataCCW : vertexDataCW;
    
    static const std::vector<std::byte> vertices(
        reinterpret_cast<const std::byte*>(vertexData),
        reinterpret_cast<const std::byte*>(vertexData) + sizeof(vertexData)
    );

    RenderTriangles(vertices, VertFormats::PosWUVInvW(), &tex);
}

void RendererCPU::RenderPipeline(const Texture* tex = nullptr) {
    PROFILE_SCOPE("RenderTrianglesCPU");

    // vertex shader stage
    {
        Logger::Debug("RenderTrianglesCPU: VertexShaderCPU started");
        PROFILE_SCOPE("RenderTrianglesCPU.VertexShaderCPU");
        _vertex_shader.UseBatch(_vertexBuffer);
        Logger::Debug("RenderTrianglesCPU: VertexShaderCPU passed");
    }

    // clipping against frustum planes
    {
        Logger::Debug("RenderTrianglesCPU: Clipping started");
        PROFILE_SCOPE("RenderTrianglesCPU.Clipping");
        ClippingHelper(_vertexBuffer, _clippedBuffer);
        std::swap(_vertexBuffer, _clippedBuffer);
        _triangles_past_clipping += static_cast<unsigned int>(_vertexBuffer.size() / 3);
        Logger::Debug("RenderTrianglesCPU: Clipping passed");
    }

    // perspective divide + viewport transform
    {
        Logger::Debug("RenderTrianglesCPU: PerspectiveTransform started");
        PROFILE_SCOPE("RenderTrianglesCPU.PerspectiveTransform");
        PerspectiveAndViewportTransform(_vertexBuffer);
        Logger::Debug("RenderTrianglesCPU: PerspectiveTransform passed");
    }

    // backface culling
    if (_renderer->_backface_culling) {
        Logger::Debug("RenderTrianglesCPU: BackfaceCulling started");
        PROFILE_SCOPE("RenderTrianglesCPU.BackfaceCulling");
        BackFaceCullHelper(_vertexBuffer);
        Logger::Debug("RenderTrianglesCPU: BackfaceCulling passed");
    }
    _triangles_past_backface_culling += static_cast<unsigned int>(_vertexBuffer.size() / 3);

    // tile management
    auto& tile_manager = TileManager::GetInst();
    {
        Logger::Debug("RenderTrianglesCPU: TileBinning.Setup started");
        PROFILE_SCOPE("RenderTrianglesCPU.TileBinning.Setup");
        if (!tile_manager.IsInitialized()) {
            tile_manager.InitializeTiles();
        }
        Logger::Debug("RenderTrianglesCPU: TileBinning.Setup passed");
    }
    {
        Logger::Debug("RenderTrianglesCPU: TileBinning.Binning started");
        PROFILE_SCOPE("RenderTrianglesCPU.TileBinning.Binning");
        tile_manager.BinTrianglesToTiles(_vertexBuffer);
        Logger::Debug("RenderTrianglesCPU: TileBinning.Binning passed");
    }

    // Precompute color LUT if not done yet
    if (!_renderer->_colorLUTComputed) {
        Logger::Debug("RenderTrianglesCPU: PrecomputeColorLUT started");
        PROFILE_SCOPE("RenderTrianglesCPU.PrecomputeColorLUT");
        _renderer->PrecomputeColorLUT();
        Logger::Debug("RenderTrianglesCPU: PrecomputeColorLUT passed");
    }

    // tile rendering
    {
        Logger::Debug("RenderTrianglesCPU: TileRasterization started");
        PROFILE_SCOPE("RenderTrianglesCPU.TileRasterization");
        DrawTiles(_vertexBuffer, tex);
        Logger::Debug("RenderTrianglesCPU: TileRasterization passed");
    }
}

void RendererCPU::RenderTriangles(const std::vector<std::vector<std::byte>*>& vertices, const VertFormat& format, const Texture* tex) {
    // CPU renderer requires PosWUVInvW format
    if (format != VertFormats::PosWUVInvW()) {
        Logger::Error("RenderTrianglesCPU: Vertices must use PosWUVInvW vertex format!");
        return;
    }

    const uint32_t stride = format.GetStride(); // 28 bytes for PosWUVInvW
    
    _vertexBuffer.clear();

    // Interpret byte data as VertStructs::PosWUVInvW and copy into buffer
    for (const auto* vvec : vertices) {
        if (!vvec || vvec->empty()) {
            if (!vvec) {
                Logger::Warning("RenderTriangles: Null vertex vector encountered in input.");
            }
            continue;
        }
        
        const VertStructs::PosWUVInvW* vertexData = reinterpret_cast<const VertStructs::PosWUVInvW*>(vvec->data());
        const size_t vertexCount = vvec->size() / stride;
        
        _vertexBuffer.insert(_vertexBuffer.end(), vertexData, vertexData + vertexCount);
    }

    const size_t totalVerts = _vertexBuffer.size();
    
    if (totalVerts < 3) {
        Logger::Warning("RenderTriangles: Not enough vertices (" + std::to_string(totalVerts) + ") to form a triangle.");
        return;
    }
    if (totalVerts % 3 != 0) {
        Logger::Warning("RenderTriangles: Vertex count (" + std::to_string(totalVerts) + ") is not a multiple of 3.");
        return;
    }

    RenderPipeline(tex);
}

void RendererCPU::RenderTriangles(const std::vector<std::byte>& vertices, const VertFormat& format, const Texture* tex) {
    // CPU renderer requires PosWUVInvW format
    if (format != VertFormats::PosWUVInvW()) {
        Logger::Error("RenderTrianglesCPU: Vertices must use PosWUVInvW vertex format!");
        return;
    }

    const uint32_t stride = format.GetStride(); // 28 bytes for PosWUVInvW
    const size_t vertexCount = vertices.size() / stride;

    // basic validation
    if (vertexCount < 3) {
        Logger::Warning("RenderTriangles: Not enough vertices (" + std::to_string(vertexCount) + ") to form a triangle.");
        return;
    }
    if (vertexCount % 3 != 0) {
        Logger::Warning("RenderTriangles: Vertex count (" + std::to_string(vertexCount) + ") is not a multiple of 3.");
        return;
    }
    
    _triangles_inputted += static_cast<unsigned int>(vertexCount / 3);
    
    // Interpret byte data as VertStructs::PosWUVInvW and copy into buffer
    {
        PROFILE_SCOPE("RenderTriangles.VertexBufferSetup");
        if (_vertexBuffer.capacity() < vertexCount) {
            _vertexBuffer.reserve(static_cast<size_t>(vertexCount * 1.5f));
        }
        
        const VertStructs::PosWUVInvW* vertexData = reinterpret_cast<const VertStructs::PosWUVInvW*>(vertices.data());
        _vertexBuffer.assign(vertexData, vertexData + vertexCount);
    }

    RenderPipeline(tex);
}

void RendererCPU::GenerateSubPixelOffsets(int sampleCount) {
    std::vector<std::pair<float, float>> offsets;
    
    if (sampleCount == 1) {
        // Single sample at pixel center
        offsets = {{0.0f, 0.0f}};
    } else {
        // Generalized grid-based sampling that ensures good edge coverage for ANY sample count
        int gridSize = (int)std::ceil(std::sqrt(sampleCount));
        
        // Use 85% of pixel area for sampling to ensure good edge detection
        // while avoiding sampling too close to adjacent pixels
        float sampleRange = 0.85f;
        float stepSize = sampleRange / gridSize;
        float startOffset = -sampleRange * 0.5f + stepSize * 0.5f;
        
        // Generate grid pattern with proper spacing for the exact sample count
        for (int i = 0; i < sampleCount; i++) {
            int x = i % gridSize;
            int y = i / gridSize;
            
            float offsetX = startOffset + x * stepSize;
            float offsetY = startOffset + y * stepSize;
            
            offsets.push_back({offsetX, offsetY});
        }
    }
    
    _subpixel_offsets = std::move(offsets);
}

const std::vector<std::pair<float, float>>& RendererCPU::GetSubpixelOffsets() {
    if (_subpixel_offsets.empty()) {
        GenerateSubPixelOffsets(_renderer->_antialiasing_samples);
    }
    return _subpixel_offsets;
}

void RendererCPU::BackFaceCullHelper(std::vector<VertStructs::PosWUVInvW>& vertices) {
    if (vertices.size() < 3) return;

    const size_t triangleCount = vertices.size() / 3;
    
    // Create a parallel char array to mark which triangles to keep (thread-safe)
    std::vector<char> keepTriangle(triangleCount);
    
    // Create index vector for parallel processing
    std::vector<size_t> triangleIndices(triangleCount);
    std::iota(triangleIndices.begin(), triangleIndices.end(), 0);
    
    // Adaptive culling: parallel for large meshes, sequential for small meshes
    if (triangleCount >= 1000) {
        // Parallel pass for large meshes (thread overhead is worth it)
        tbb::parallel_for_each(
            triangleIndices.begin(), 
            triangleIndices.end(),
            [this, &keepTriangle, &vertices, ccw = _renderer->_ccw](size_t triIndex) {
                const size_t vertexIndex = triIndex * 3;
                keepTriangle[triIndex] = !BackFaceCull(
                    vertices[vertexIndex], 
                    vertices[vertexIndex + 1], 
                    vertices[vertexIndex + 2], 
                    ccw
                );
            });
    } else {
        // Sequential pass for small meshes (avoid thread overhead)
        for (size_t triIndex = 0; triIndex < triangleCount; ++triIndex) {
            const size_t vertexIndex = triIndex * 3;
            keepTriangle[triIndex] = !BackFaceCull(
                vertices[vertexIndex], 
                vertices[vertexIndex + 1], 
                vertices[vertexIndex + 2], 
                _renderer->_ccw
            );
        }
    }
    
    // In-place removal: shift kept triangles to the front
    size_t writeIndex = 0;
    for (size_t triIndex = 0; triIndex < triangleCount; ++triIndex) {
        if (keepTriangle[triIndex]) {
            const size_t readIndex = triIndex * 3;
            if (writeIndex != readIndex) {
                // Only move if positions differ
                vertices[writeIndex]     = vertices[readIndex];
                vertices[writeIndex + 1] = vertices[readIndex + 1];
                vertices[writeIndex + 2] = vertices[readIndex + 2];
            }
            writeIndex += 3;
        }
    }
    
    // Resize to remove culled triangles
    vertices.resize(writeIndex);
}

// =============================================================================
// TILE-BASED RENDERING FUNCTIONS
// =============================================================================

void RendererCPU::DrawTiles(const std::vector<VertStructs::PosWUVInvW>& raster_triangles, const Texture* tex) {
    auto& tile_manager = TileManager::GetInst();
    tile_manager.UpdateActiveTiles();
    
    if (!tile_manager.activeTiles.empty()) {
        const size_t tileCount = tile_manager.activeTiles.size();
        
        // Adaptive: parallel for many tiles (16+), sequential for few tiles
        if (tileCount >= 16) {
            tbb::parallel_for_each(tile_manager.activeTiles.begin(), tile_manager.activeTiles.end(),
                [&](Tile* tile) {
                    if (_renderer->_wireframe || tex == nullptr) {
                        DrawTileWireframe(*tile, raster_triangles);
                    } else {
                        DrawTileTextured(*tile, raster_triangles, tex);
                    }
                });
        } else {
            // Sequential for small tile counts (avoid thread overhead)
            for (Tile* tile : tile_manager.activeTiles) {
                if (_renderer->_wireframe || tex == nullptr) {
                    DrawTileWireframe(*tile, raster_triangles);
                } else {
                    DrawTileTextured(*tile, raster_triangles, tex);
                }
            }
        }
    }
}

void RendererCPU::DrawTileTextured(const Tile& tile, const std::vector<VertStructs::PosWUVInvW>& raster_triangles, const Texture* tex) {
    for (int triIndex : tile.tri_indices_encapsulated) {
        DrawTriangleTextured(raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
    }
    
    for (int triIndex : tile.tri_indices_partial) {
        DrawTriangleTexturedPartial(tile, raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
    }
}

void RendererCPU::DrawTileWireframe(const Tile& tile, const std::vector<VertStructs::PosWUVInvW>& raster_triangles) {
    // Draw wireframe for each triangle in the tile
    for (int triIndex : tile.tri_indices_encapsulated) {
        _renderer->DrawTriangleWireframeColBuff(raster_triangles[triIndex].GetXY(), raster_triangles[triIndex + 1].GetXY(), raster_triangles[triIndex + 2].GetXY(), glm::ivec3(15));
    }
    for (int triIndex : tile.tri_indices_partial) {
        DrawTriangleWireframeColBuffPartial(tile, raster_triangles[triIndex].GetXY(), raster_triangles[triIndex + 1].GetXY(), raster_triangles[triIndex + 2].GetXY(), glm::ivec4(15));
    }
}

void RendererCPU::DrawTriangleWireframeColBuffPartial(const Tile& tile, const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::vec4& col) {
    // Get tile bounds for clipping
    const int minX = static_cast<int>(tile.position.x);
    const int maxX = static_cast<int>(tile.position.x + tile.size.x);
    const int minY = static_cast<int>(tile.position.y);
    const int maxY = static_cast<int>(tile.position.y + tile.size.y);
    
    // Draw the three edges of the triangle with tile clipping
    _renderer->DrawClippedLineColBuff(static_cast<int>(vert1.x), static_cast<int>(vert1.y), 
                    static_cast<int>(vert2.x), static_cast<int>(vert2.y),
                    minX, maxX, minY, maxY, col);
    _renderer->DrawClippedLineColBuff(static_cast<int>(vert2.x), static_cast<int>(vert2.y), 
                    static_cast<int>(vert3.x), static_cast<int>(vert3.y),
                    minX, maxX, minY, maxY, col);
    _renderer->DrawClippedLineColBuff(static_cast<int>(vert3.x), static_cast<int>(vert3.y),
                    static_cast<int>(vert1.x), static_cast<int>(vert1.y),
                    minX, maxX, minY, maxY, col);
}

void RendererCPU::DrawTriangleTexturedPartial(const Tile& tile, const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, const Texture* tex) {
    if (!tex) { Logger::Error("  Texture is nullptr!"); return; }
    const int texWidth = tex->GetWidth(), texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0) { Logger::Error("  Invalid texture dimensions!"); return; }

    // Cache screen dimensions
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();

    // Hoist tile bounds
    const int tileMinX = static_cast<int>(tile.position.x);
    const int tileMaxX = static_cast<int>(tile.position.x + tile.size.x) - 1;
    const int tileMinY = static_cast<int>(tile.position.y);
    const int tileMaxY = static_cast<int>(tile.position.y + tile.size.y) - 1;

    // Get triangle bounding box clipped to tile (use floor/ceil for consistency)
    int minX = std::max(tileMinX, static_cast<int>(std::floor(std::min({vert1.X(), vert2.X(), vert3.X()}))));
    int maxX = std::min(tileMaxX, static_cast<int>(std::ceil(std::max({vert1.X(), vert2.X(), vert3.X()}))));
    int minY = std::max(tileMinY, static_cast<int>(std::floor(std::min({vert1.Y(), vert2.Y(), vert3.Y()}))));
    int maxY = std::min(tileMaxY, static_cast<int>(std::ceil(std::max({vert1.Y(), vert2.Y(), vert3.Y()}))));
    minX = std::max(0, minX);
    maxX = std::min(screenWidth - 1, maxX);
    minY = std::max(0, minY);
    maxY = std::min(screenHeight - 1, maxY);
    if (minX > maxX || minY > maxY) return;

    // Call shared implementation
    DrawTriangleTexturedImpl(vert1, vert2, vert3, tex, minX, maxX, minY, maxY);
}

void RendererCPU::DrawTriangleTexturedImpl(const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, const Texture* tex, int minX, int maxX, int minY, int maxY) {
    const int texWidth = tex->GetWidth(), texHeight = tex->GetHeight();
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();

    // Vertex positions
    const float x1 = vert1.X(), y1 = vert1.Y();
    const float x2 = vert2.X(), y2 = vert2.Y();
    const float x3 = vert3.X(), y3 = vert3.Y();

    // Signed doubled area
    const float area2 = (x2 - x1)*(y3 - y1) - (x3 - x1)*(y2 - y1);
    if (fabsf(area2) < 1e-7f) return;

    // Perspective-correct attributes
    const float w1p = vert1.InvW(), w2p = vert2.InvW(), w3p = vert3.InvW();
    const float u1 = vert1.U(),   u2 = vert2.U(),   u3 = vert3.U();
    const float v1t = vert1.V(),  v2t = vert2.V(),  v3t = vert3.V();

    // Edge function coefficients (A x + B y + C)
    auto edgeCoeff = [](float xA, float yA, float xB, float yB, float& A, float& B, float& C){
        A = yA - yB; B = xB - xA; C = xA * yB - xB * yA; };
    float A12,B12,C12, A23,B23,C23, A31,B31,C31;
    edgeCoeff(x1,y1,x2,y2,A12,B12,C12);
    edgeCoeff(x2,y2,x3,y3,A23,B23,C23);
    edgeCoeff(x3,y3,x1,y1,A31,B31,C31);

    // Make winding consistent (inside = all edges >= 0)
    const bool flip = (area2 < 0.0f);
    auto orient = [&](float& A,float& B,float& C){ if (flip){ A=-A; B=-B; C=-C; } };
    float signedArea = flip ? -area2 : area2;
    orient(A12,B12,C12); orient(A23,B23,C23); orient(A31,B31,C31);
    const float invSignedArea = 1.0f / signedArea;
    
    const float u1_invArea = u1 * invSignedArea;
    const float u2_invArea = u2 * invSignedArea;
    const float u3_invArea = u3 * invSignedArea;
    const float v1t_invArea = v1t * invSignedArea;
    const float v2t_invArea = v2t * invSignedArea;
    const float v3t_invArea = v3t * invSignedArea;
    const float w1p_invArea = w1p * invSignedArea;
    const float w2p_invArea = w2p * invSignedArea;
    const float w3p_invArea = w3p * invSignedArea;

    // Choose sampling pattern based on antialiasing setting
    // When AA is off, use single sample at pixel center (0.0, 0.0 offset)
    static const std::vector<std::pair<float, float>> centerSample = {{0.0f, 0.0f}};
    const auto& subPixelOffsets = _renderer->_antialiasing ? GetSubpixelOffsets() : centerSample;
    const int sampleCount = (int)subPixelOffsets.size();

    for (int y = minY; y <= maxY; ++y) {
        const float baseY = y + 0.5f;
        const size_t rowOffset = y * screenWidth;
        
        // Initialize edge functions at start of row (x = minX + 0.5)
        const float startX = minX + 0.5f;
        float e12c = A12 * startX + B12 * baseY + C12;
        float e23c = A23 * startX + B23 * baseY + C23;
        float e31c = A31 * startX + B31 * baseY + C31;
        
        for (int x = minX; x <= maxX; ++x) {
            const float baseX = x + 0.5f;

            if (e12c < -1.1f && e23c < -1.1f && e31c < -1.1f) {
                // Increment edge functions for next pixel
                e12c += A12;
                e23c += A23;
                e31c += A31;
                continue;
            }

            // Pre-fetch current depth for early rejection
            const size_t depthIndex = rowOffset + x;
            const float currentDepth = _depth_buffer[depthIndex];

            glm::vec4 rgbAccum(0.0f); 
            float depthAccum = 0.0f; 
            int insideSamples = 0;

            for (int si = 0; si < sampleCount; ++si) {
                const float sx = baseX + subPixelOffsets[si].first;
                const float sy = baseY + subPixelOffsets[si].second;
                const float e12 = A12 * sx + B12 * sy + C12;
                const float e23 = A23 * sx + B23 * sy + C23;
                const float e31 = A31 * sx + B31 * sy + C31;
                if (e12 < 0.f || e23 < 0.f || e31 < 0.f) continue;
                
                const float wA_times_area = e23;
                const float wB_times_area = e31;
                const float wC_times_area = signedArea - e23 - e31;
                
                const float perspW = wA_times_area * w1p_invArea + wB_times_area * w2p_invArea + wC_times_area * w3p_invArea;
                
                // Early depth test BEFORE expensive texture sampling
                if (perspW <= currentDepth) continue;
                
                const float invPerspW = 1.0f / perspW;
                
                const float tu = (wA_times_area * u1_invArea + wB_times_area * u2_invArea + wC_times_area * u3_invArea) * invPerspW;
                const float tv = (wA_times_area * v1t_invArea + wB_times_area * v2t_invArea + wC_times_area * v3t_invArea) * invPerspW;
                
                const int texelX = static_cast<int>(tu * texWidth);
                const int texelY = static_cast<int>(tv * texHeight);
                
                if (static_cast<unsigned>(texelX) >= static_cast<unsigned>(texWidth) || 
                    static_cast<unsigned>(texelY) >= static_cast<unsigned>(texHeight)) continue;

                const uint8_t* pixel = tex->GetPixelRGBAPtr(texelX, texelY);
                rgbAccum.r += pixel[0];
                rgbAccum.g += pixel[1];
                rgbAccum.b += pixel[2];
                rgbAccum.a += pixel[3];
                
                depthAccum += perspW;
                ++insideSamples;
            }

            if (insideSamples) {
                const float inv_inside_samples = 1.0f / insideSamples;
                const float avgDepth = depthAccum * inv_inside_samples;
                const glm::ivec4 c = glm::ivec4(rgbAccum * inv_inside_samples + 0.5f);
                PlotColorBlend(x, y, c, avgDepth);
            }
            
            // Increment edge functions for next pixel (moving right by 1.0)
            e12c += A12;
            e23c += A23;
            e31c += A31;
        }
    }
}

void RendererCPU::DrawTriangleTextured(const VertStructs::PosWUVInvW& v1, const VertStructs::PosWUVInvW& v2, const VertStructs::PosWUVInvW& v3, const Texture* tex) {
    if (!tex) { Logger::Error("  Texture is nullptr!"); return; }
    const int texWidth = tex->GetWidth(), texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0) { Logger::Error("  Invalid texture dimensions!"); return; }

    // Cache screen dimensions
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();

    // Triangle bounding box (clamped)
    int minX = std::max(0, (int)std::floor(std::min({ v1.X(), v2.X(), v3.X() })));
    int maxX = std::min(screenWidth - 1, (int)std::ceil (std::max({ v1.X(), v2.X(), v3.X() })));
    int minY = std::max(0, (int)std::floor(std::min({ v1.Y(), v2.Y(), v3.Y() })));
    int maxY = std::min(screenHeight - 1, (int)std::ceil (std::max({ v1.Y(), v2.Y(), v3.Y() })));
    if (minX > maxX || minY > maxY) return;

    // Call shared implementation
    DrawTriangleTexturedImpl(v1, v2, v3, tex, minX, maxX, minY, maxY);
}

bool RendererCPU::BackFaceCull(const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, bool CCW) { // determines if the triangle is in the correct winding order or not

	// it calculates it based on glm cross because if the perpendicular z is less than 0, the triangle is pointing away
	glm::vec3 U = vert2.GetXYZ() - vert1.GetXYZ();
	glm::vec3 V = vert3.GetXYZ() - vert1.GetXYZ();

	float crossZ;

	if (CCW == true)
		crossZ = glm::cross(U, V).z;
	else
		crossZ = glm::cross(V, U).z;

	return crossZ > 0.0f;
}

// =============================================================================
// CLIPPING FUNCTIONS
// =============================================================================

void RendererCPU::ClippingHelper(std::vector<VertStructs::PosWUVInvW>& vertices, std::vector<VertStructs::PosWUVInvW>& clipped) {
    if (vertices.size() < 3) return;

    // Adaptive clipping: use multithreading for large meshes, single-threaded for small meshes
    if (vertices.size() >= 6000) {
        Logger::Debug("ClippingHelper: Using multithreaded clipping for large mesh");
        ClippingHelperThreaded(vertices, clipped);
    } else {
        Logger::Debug("ClippingHelper: Using single-threaded clipping for small mesh");
        ClippingHelperSingleThreaded(vertices, clipped);
    }
}

void RendererCPU::ClippingHelperThreaded(std::vector<VertStructs::PosWUVInvW>& vertices, std::vector<VertStructs::PosWUVInvW>& clipped) {
    constexpr int NUM_PLANES = 6;
    static const int components[NUM_PLANES] = {2, 2, 1, 1, 0, 0};
    static const bool nears[NUM_PLANES]     = {true, false, true, false, true, false};

    clipped.clear();
    const size_t triangleCount = vertices.size() / 3;
    if (triangleCount == 0) return;

    // --- TBB parallel setup ---
    const size_t numThreads = tbb::this_task_arena::max_concurrency();
    const size_t batchSize  = (triangleCount + numThreads - 1) / numThreads;

    std::vector<std::vector<VertStructs::PosWUVInvW>> threadResults(numThreads);

    // --- Parallel work using TBB ---
    tbb::parallel_for(size_t(0), numThreads, [&](size_t t) {
        std::vector<VertStructs::PosWUVInvW>& out = threadResults[t];
        out.clear();
        out.reserve(batchSize * 3); // heuristic: ~3 verts per tri

        std::vector<VertStructs::PosWUVInvW> tri;
        std::vector<VertStructs::PosWUVInvW> temp;
        tri.reserve(12);
        temp.reserve(12);

        const size_t start = t * batchSize;
        const size_t end   = std::min(triangleCount, start + batchSize);

        for (size_t triIdx = start; triIdx < end; ++triIdx) {
            tri.clear();
            tri.push_back(vertices[triIdx * 3 + 0]);
            tri.push_back(vertices[triIdx * 3 + 1]);
            tri.push_back(vertices[triIdx * 3 + 2]);

            for (int planeIdx = 0; planeIdx < NUM_PLANES; ++planeIdx) {
                temp.clear();
                for (size_t i = 0; i + 2 < tri.size(); i += 3) {
                    ClipTriAgainstPlane(
                        tri[i], tri[i + 1], tri[i + 2],
                        temp, components[planeIdx], nears[planeIdx]
                    );
                }
                if (temp.empty()) { tri.clear(); break; }
                tri.swap(temp);
            }

            if (!tri.empty()) {
                out.insert(out.end(), tri.begin(), tri.end());
            }
        }
    });

    // --- Merge results ---
    size_t total = 0;
    for (auto& buf : threadResults) total += buf.size();
    clipped.reserve(total);
    for (auto& buf : threadResults) {
        clipped.insert(clipped.end(), buf.begin(), buf.end());
    }
}

void RendererCPU::ClippingHelperSingleThreaded(std::vector<VertStructs::PosWUVInvW>& vertices, std::vector<VertStructs::PosWUVInvW>& clipped) {
    constexpr int NUM_PLANES = 6;
    static const int components[NUM_PLANES] = {2, 2, 1, 1, 0, 0};
    static const bool nears[NUM_PLANES]     = {true, false, true, false, true, false};

    clipped.clear();
    const size_t triangleCount = vertices.size() / 3;
    if (triangleCount == 0) return;

    std::vector<VertStructs::PosWUVInvW> tri;
    std::vector<VertStructs::PosWUVInvW> temp;
    tri.reserve(12);
    temp.reserve(12);

    for (size_t triIdx = 0; triIdx < triangleCount; ++triIdx) {
        tri.clear();
        temp.clear();

        // Copy triangle
        tri.push_back(vertices[triIdx * 3 + 0]);
        tri.push_back(vertices[triIdx * 3 + 1]);
        tri.push_back(vertices[triIdx * 3 + 2]);

        // Clip against each plane
        for (int planeIdx = 0; planeIdx < NUM_PLANES; ++planeIdx) {
            temp.clear();
            for (size_t i = 0; i + 2 < tri.size(); i += 3) {
                ClipTriAgainstPlane(tri[i], tri[i + 1], tri[i + 2],
                                    temp, components[planeIdx], nears[planeIdx]);
            }
            if (temp.empty()) { tri.clear(); break; }
            tri.swap(temp);
        }

        if (!tri.empty()) {
            clipped.insert(clipped.end(), tri.begin(), tri.end());
        }
    }
}

void RendererCPU::ClipTriAgainstPlane(const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3, std::vector<VertStructs::PosWUVInvW>& output, 
                                const int component, const bool Near) {

    const float w0 = vert1.W();
    const float w1 = vert2.W();
    const float w2 = vert3.W();
    const float val0 = vert1.data[component];
    const float val1 = vert2.data[component];
    const float val2 = vert3.data[component];

    const bool in0 = Near ? (val0 > -w0) : (val0 < w0);
    const bool in1 = Near ? (val1 > -w1) : (val1 < w1);
    const bool in2 = Near ? (val2 > -w2) : (val2 < w2);

    const int insideCount = (in0 ? 1 : 0) + (in1 ? 1 : 0) + (in2 ? 1 : 0);

    // All vertices inside - keep triangle as-is (most common case first)
    if (insideCount == 3) {
        output.push_back(vert1);
        output.push_back(vert2);
        output.push_back(vert3);
    }
    // Two vertices inside - create 2 triangles (quad split)
    else if (insideCount == 2) {
        VertStructs::PosWUVInvW insideVert0, insideVert1, outsideVert;

        if (!in0) {
            outsideVert = vert1; insideVert0 = vert2; insideVert1 = vert3;
        } else if (!in1) {
            outsideVert = vert2; insideVert0 = vert3; insideVert1 = vert1;
        } else { // !in2
            outsideVert = vert3; insideVert0 = vert1; insideVert1 = vert2;
        }

        const VertStructs::PosWUVInvW newVert0 = HomogenousPlaneIntersect(insideVert0, outsideVert, component, Near);
        const VertStructs::PosWUVInvW newVert1 = HomogenousPlaneIntersect(insideVert1, outsideVert, component, Near);

        // Triangle 1
        output.push_back(insideVert0);
        output.push_back(insideVert1);
        output.push_back(newVert0);

        // Triangle 2
        output.push_back(insideVert1);
        output.push_back(newVert1);
        output.push_back(newVert0);
    }
    // One vertex inside - create 1 smaller triangle
    else if (insideCount == 1) {
        VertStructs::PosWUVInvW insideVert, outsideVert0, outsideVert1;

        if (in0) {
            insideVert = vert1; outsideVert0 = vert2; outsideVert1 = vert3;
        } else if (in1) {
            insideVert = vert2; outsideVert0 = vert3; outsideVert1 = vert1;
        } else { // in2
            insideVert = vert3; outsideVert0 = vert1; outsideVert1 = vert2;
        }

        const VertStructs::PosWUVInvW newVert0 = HomogenousPlaneIntersect(insideVert, outsideVert0, component, Near);
        const VertStructs::PosWUVInvW newVert1 = HomogenousPlaneIntersect(insideVert, outsideVert1, component, Near);

        output.push_back(insideVert);
        output.push_back(newVert0);
        output.push_back(newVert1);
    }
}

bool RendererCPU::IsInitialized() const {
    return _initialized;
}

void RendererCPU::PlotColorBlend(int x, int y, const glm::ivec4& color, float depth) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    const int idx = y * screenWidth + x;
    
    // for now since we don't have depth ordering, it just assumes everything is opaque
    if (depth > _depth_buffer[idx]) {
        _renderer->_color_buffer[idx] = color;
        _depth_buffer[idx] = depth;
    }
}

void RendererCPU::PerspectiveAndViewportTransform(std::vector<VertStructs::PosWUVInvW>& vertices) {
    const float screenWidth = static_cast<float>(Screen::GetInst().GetWidth());
    const float screenHeight = static_cast<float>(Screen::GetInst().GetHeight());
    const float halfWidth = screenWidth * 0.5f;
    const float halfHeight = screenHeight * 0.5f;

    for (auto& v : vertices) {
        const float w = v.W();
        const float invW = 1.0f / w;
        
        // Combined perspective division + viewport transform
        v.data[0] = ((v.data[0] * invW) + 1.0f) * halfWidth;   // x: perspective + viewport
        v.data[1] = (1.0f - (v.data[1] * invW)) * halfHeight;  // y: perspective + viewport + flip
        v.data[2] *= invW;
        
        v.data[4] *= invW; // u
        v.data[5] *= invW; // v  
        v.data[6] = invW;  // w becomes 1/w
    }
}

void RendererCPU::BeginColBuffFrame() {
    std::fill(_depth_buffer.begin(), _depth_buffer.end(), -FLT_MAX);
}

VERTEX_SHADER_CPU& RendererCPU::GetVShader() {
    return _vertex_shader;
}

} // namespace ASCIIgL