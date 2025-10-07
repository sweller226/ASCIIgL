#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>
#include <execution>
#include <numeric>
#include <sstream>

#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>

#include <ASCIIgL/renderer/Screen.hpp>

// =============================================================================
// CONSTRUCTION / DESTRUCTION / INITIALIZATION
// =============================================================================

Renderer::~Renderer() {
    _depth_buffer.clear();
    _color_buffer.clear();
}

void Renderer::Initialize() {
    if (!Screen::GetInst().IsInitialized()) {
        Logger::Error("Renderer: Screen must be initialized before creating Renderer.");
        throw std::runtime_error("Renderer: Screen must be initialized before creating Renderer.");
    }
    auto& screen = Screen::GetInst();
    _depth_buffer.resize(screen.GetWidth() * screen.GetHeight());
    _color_buffer.resize(screen.GetWidth() * screen.GetHeight());

    _vertexBuffer.reserve(100000);  // Enough for ~33k triangles
    _clippedBuffer.reserve(200000); // Clipping can double vertices
}

// =============================================================================
// PUBLIC HIGH-LEVEL DRAWING FUNCTIONS
// =============================================================================

void Renderer::DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh) {
    if (mesh->texture) {
        RenderTriangles(VSHADER, mesh->vertices, mesh->texture);
    }
}

void Renderer::DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const Camera3D& camera) {
    if (!mesh) {
        Logger::Error("DrawMesh: mesh is nullptr!");
        return;
    }

    VSHADER.SetMatrices(glm::mat4(1.0f), camera.view, camera.proj);

    if (mesh->texture) {
        RenderTriangles(VSHADER, mesh->vertices, mesh->texture);
    } else {
        Logger::Warning("DrawMesh: mesh texture is null");
    }
}

void Renderer::DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera) {
    glm::mat4 model = CalcModelMatrix(position, rotation, size);

    VSHADER.SetMatrices(model, camera.view, camera.proj);

    if (mesh->texture) {
        RenderTriangles(VSHADER, mesh->vertices, mesh->texture);
    }
}



void Renderer::DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera) {
    glm::mat4 model = CalcModelMatrix(position, rotation, size);

    VSHADER.SetMatrices(model, camera.view, camera.proj);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(VSHADER, ModelObj.meshes[i]);
    }
}

void Renderer::DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4& model, const Camera3D& camera)
{
    VSHADER.SetMatrices(model, camera.view, camera.proj);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(VSHADER, ModelObj.meshes[i]);
    }
}

void Renderer::Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer)
{
    glm::mat4 model = CalcModelMatrix(glm::vec3(position, layer), rotation, glm::vec3(size, 0.0f));

    VSHADER.SetMatrices(model, camera.view, camera.proj);

    std::vector<VERTEX> vertices = {
        VERTEX({ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
        VERTEX({ -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
        VERTEX({  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
        VERTEX({  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
        VERTEX({  1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
        VERTEX({ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
    };

    Renderer::RenderTriangles(VSHADER, vertices, &tex);
}

void Renderer::Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer)
{
    // Convert percentage coordinates to pixel coordinates
    float screenWidth = static_cast<float>(Screen::GetInst().GetWidth());
    float screenHeight = static_cast<float>(Screen::GetInst().GetHeight());
    
    glm::vec2 pixelPosition = glm::vec2(positionPerc.x * screenWidth, positionPerc.y * screenHeight);
    glm::vec2 pixelSize = glm::vec2(sizePerc.x * screenWidth, sizePerc.y * screenHeight);
    
    glm::mat4 model = CalcModelMatrix(glm::vec3(pixelPosition, layer), rotation, glm::vec3(pixelSize, 0.0f));

    VSHADER.SetModel(model);
    VSHADER.SetView(camera.view);
    VSHADER.SetProj(camera.proj);
    VSHADER.UpdateMVP();

    std::vector<VERTEX> vertices = {
        VERTEX({ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
        VERTEX({ -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f}), // top-left
        VERTEX({  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
        VERTEX({  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f}), // top-right
        VERTEX({  1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f}), // bottom-right
        VERTEX({ -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f}), // bottom-left
    };

    Renderer::RenderTriangles(VSHADER, vertices, &tex);
}

void Renderer::DrawScreenBorderPxBuff(const unsigned short col) {
	// DRAWING BORDERS
	DrawLinePxBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, _charRamp[8], col);
	DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, _charRamp[8], col);
	DrawLinePxBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, _charRamp[8], col);
	DrawLinePxBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, _charRamp[8], col);
}

void Renderer::DrawScreenBorderColBuff(const glm::vec3& col) {
    // DRAWING BORDERS
    DrawLineColBuff(0, 0, Screen::GetInst().GetWidth() - 1, 0, col);
    DrawLineColBuff(Screen::GetInst().GetWidth() - 1, 0, Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, col);
    DrawLineColBuff(Screen::GetInst().GetWidth() - 1, Screen::GetInst().GetHeight() - 1, 0, Screen::GetInst().GetHeight() - 1, col);
    DrawLineColBuff(0, 0, 0, Screen::GetInst().GetHeight() - 1, col);
}

// =============================================================================
// CORE RENDERING PIPELINE
// =============================================================================

void Renderer::RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex) {
    if (vertices.size() < 3 || vertices.size() % 3 != 0) {
        return;
    }
    _triangles_inputted += static_cast<unsigned int>(vertices.size() / 3);

    if (tex && (tex->GetWidth() <= 0 || tex->GetHeight() <= 0)) {
        return;
    }

    if (_vertexBuffer.capacity() < vertices.size()) {
        _vertexBuffer.reserve(vertices.size() * 1.5f);
    }
    _vertexBuffer.assign(vertices.begin(), vertices.end()); 

    const_cast<VERTEX_SHADER&>(VSHADER).GLUseBatch(_vertexBuffer);

    ClippingHelper(_vertexBuffer, _clippedBuffer);
    std::swap(_vertexBuffer, _clippedBuffer);
    _triangles_past_clipping += static_cast<unsigned int>(_vertexBuffer.size() / 3);

    PerspectiveAndViewportTransform(_vertexBuffer);

    if (_backface_culling) {
        BackFaceCullHelper(_vertexBuffer);
    }
    _triangles_past_backface_culling += static_cast<unsigned int>(_vertexBuffer.size() / 3);

    // tile management
    auto& tile_manager = TileManager::GetInst();
    if (!tile_manager.IsInitialized()) {
        tile_manager.InitializeTiles(Screen::GetInst().GetWidth(), Screen::GetInst().GetHeight());
    }
    tile_manager.BinTrianglesToTiles(_vertexBuffer); // Now includes clearing

    if (!_colorLUTComputed) {
        PrecomputeColorLUT();
    }

    // tile rendering
    tile_manager.UpdateActiveTiles();
    if (!tile_manager.activeTiles.empty()) {
        std::for_each(std::execution::par, tile_manager.activeTiles.begin(), tile_manager.activeTiles.end(),
            [&](Tile* tile) {
                if (_wireframe || tex == nullptr) {
                    DrawTileWireframe(*tile, _vertexBuffer);
                } else {
                    DrawTileTextured(*tile, _vertexBuffer, tex);
                }
            });
    }
}

void Renderer::BackFaceCullHelper(std::vector<VERTEX>& vertices) {
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
        std::for_each(std::execution::par, 
            triangleIndices.begin(), 
            triangleIndices.end(),
            [this, &keepTriangle, &vertices, ccw = _ccw](size_t triIndex) {
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
                _ccw
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

void Renderer::DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex) {
    for (int triIndex : tile.tri_indices_encapsulated) {
        DrawTriangleTextured(raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
    }
    
    for (int triIndex : tile.tri_indices_partial) {
        DrawTriangleTexturedPartial(tile, raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
    }
}

void Renderer::DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles) {
    // Draw wireframe for each triangle in the tile
    for (int triIndex : tile.tri_indices_encapsulated) {
        DrawTriangleWireframeColBuff(raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    for (int triIndex : tile.tri_indices_partial) {
        DrawTriangleWireframeColBuffPartial(tile, raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}

// =============================================================================
// TRIANGLE RASTERIZATION FUNCTIONS
// =============================================================================

void Renderer::DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col) {
    // Bresenham's line algorithm with tile bounds clipping
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int incx = (x1 > x0) ? 1 : -1;
    int incy = (y1 > y0) ? 1 : -1;
    int x = x0, y = y0;

    // Plot first pixel if within bounds
    if (x >= minX && x < maxX && y >= minY && y < maxY) {
        Screen::GetInst().PlotPixel(x, y, pixel_type, col);
    }

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                Screen::GetInst().PlotPixel(x, y, pixel_type, col);
            }
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                Screen::GetInst().PlotPixel(x, y, pixel_type, col);
            }
        }
    }
}

void Renderer::DrawClippedLineColBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, const glm::vec4& col) {
    // Bresenham's line algorithm with tile bounds clipping
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int incx = (x1 > x0) ? 1 : -1;
    int incy = (y1 > y0) ? 1 : -1;
    int x = x0, y = y0;

    // Plot first pixel if within bounds
    if (x >= minX && x < maxX && y >= minY && y < maxY) {
        PlotColorBlend(x, y, col, 1.0f);
    }

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                PlotColorBlend(x, y, col, 1.0f);
            }
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            // Only plot if within tile bounds
            if (x >= minX && x < maxX && y >= minY && y < maxY) {
                PlotColorBlend(x, y, col, 1.0f);
            }
        }
    }
}

void Renderer::DrawTriangleWireframeColBuffPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col) {
    // Get tile bounds for clipping
    const int minX = static_cast<int>(tile.position.x);
    const int maxX = static_cast<int>(tile.position.x + tile.size.x);
    const int minY = static_cast<int>(tile.position.y);
    const int maxY = static_cast<int>(tile.position.y + tile.size.y);
    
    // Draw the three edges of the triangle with tile clipping
    DrawClippedLineColBuff(static_cast<int>(vert1.X()), static_cast<int>(vert1.Y()), 
                    static_cast<int>(vert2.X()), static_cast<int>(vert2.Y()),
                    minX, maxX, minY, maxY, col);
    DrawClippedLineColBuff(static_cast<int>(vert2.X()), static_cast<int>(vert2.Y()), 
                    static_cast<int>(vert3.X()), static_cast<int>(vert3.Y()),
                    minX, maxX, minY, maxY, col);
    DrawClippedLineColBuff(static_cast<int>(vert3.X()), static_cast<int>(vert3.Y()),
                    static_cast<int>(vert1.X()), static_cast<int>(vert1.Y()),
                    minX, maxX, minY, maxY, col);
}

void Renderer::DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
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

    // Vertex positions
    const float x1 = vert1.X(), y1 = vert1.Y();
    const float x2 = vert2.X(), y2 = vert2.Y();
    const float x3 = vert3.X(), y3 = vert3.Y();

    // Signed doubled area
    const float area2 = (x2 - x1)*(y3 - y1) - (x3 - x1)*(y2 - y1);
    if (fabsf(area2) < 1e-7f) return;

    // Perspective-correct attributes
    const float w1p = vert1.UVW(), w2p = vert2.UVW(), w3p = vert3.UVW();
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
    const auto& subPixelOffsets = _antialiasing ? GetSubpixelOffsets() : centerSample;
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

void Renderer::DrawTriangleTextured(const VERTEX& v1, const VERTEX& v2, const VERTEX& v3, const Texture* tex) {
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

    // Vertex positions
    const float x1 = v1.X(), y1 = v1.Y();
    const float x2 = v2.X(), y2 = v2.Y();
    const float x3 = v3.X(), y3 = v3.Y();

    // Signed doubled area
    const float area2 = (x2 - x1)*(y3 - y1) - (x3 - x1)*(y2 - y1);
    if (fabsf(area2) < 1e-7f) return;

    // Perspective-correct attributes
    const float w1p = v1.UVW(), w2p = v2.UVW(), w3p = v3.UVW();
    const float u1 = v1.U(),   u2 = v2.U(),   u3 = v3.U();
    const float v1t = v1.V(),  v2t = v2.V(),  v3t = v3.V();

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
    const auto& subPixelOffsets = _antialiasing ? GetSubpixelOffsets() : centerSample;
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

void Renderer::DrawLinePxBuff(const int x1, const int y1, const int x2, const int y2, const WCHAR pixel_type, const unsigned short col) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int incx = (x2 > x1) ? 1 : -1;
    int incy = (y2 > y1) ? 1 : -1;
    int x = x1, y = y1;

    Screen::GetInst().PlotPixel(x, y, pixel_type, col);

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            Screen::GetInst().PlotPixel(x, y, pixel_type, col);
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            Screen::GetInst().PlotPixel(x, y, pixel_type, col);
        }
    }
}

void Renderer::DrawLineColBuff(const int x1, const int y1, const int x2, const int y2, const glm::vec4& col) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int incx = (x2 > x1) ? 1 : -1;
    int incy = (y2 > y1) ? 1 : -1;
    int x = x1, y = y1;

    PlotColorBlend(x, y, col, 1.0f);

    if (dx > dy) {
        int e = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += incx;
            if (e >= 0) {
                y += incy;
                e += 2 * (dy - dx);
            } else {
                e += 2 * dy;
            }
            PlotColorBlend(x, y, col, 1.0f);
        }
    } else {
        int e = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += incy;
            if (e >= 0) {
                x += incx;
                e += 2 * (dx - dy);
            } else {
                e += 2 * dx;
            }
            PlotColorBlend(x, y, col, 1.0f);
        }
    }
}

void Renderer::DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::vec3& col) {
    DrawLineColBuff(x1, y1, x2, y2, glm::vec4(col, 1.0f));
}

void Renderer::DrawTriangleWireframePxBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const WCHAR pixel_type, const unsigned short col) {
	// RENDERING LINES BETWEEN VERTICES
	DrawLinePxBuff((int) vert1.X(), (int) vert1.Y(), (int) vert2.X(), (int) vert2.Y(), pixel_type, col);
	DrawLinePxBuff((int) vert2.X(), (int) vert2.Y(), (int) vert3.X(), (int) vert3.Y(), pixel_type, col);
	DrawLinePxBuff((int) vert3.X(), (int) vert3.Y(), (int) vert1.X(), (int) vert1.Y(), pixel_type, col);
}

void Renderer::DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec4& col) {
    // RENDERING LINES BETWEEN VERTICES
    DrawLineColBuff((int) vert1.X(), (int) vert1.Y(), (int) vert2.X(), (int) vert2.Y(), col);
    DrawLineColBuff((int) vert2.X(), (int) vert2.Y(), (int) vert3.X(), (int) vert3.Y(), col);
    DrawLineColBuff((int) vert3.X(), (int) vert3.Y(), (int) vert1.X(), (int) vert1.Y(), col);
}

void Renderer::DrawTriangleWireframeColBuff(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const glm::vec3& col) {
    DrawTriangleWireframeColBuff(vert1, vert2, vert3, glm::vec4(col, 1.0f));
}

bool Renderer::BackFaceCull(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, bool CCW) { // determines if the triangle is in the correct winding order or not

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

void Renderer::ClippingHelper(std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped) {
    static const int components[6] = {2, 2, 1, 1, 0, 0};
    static const bool nears[6]     = {true, false, true, false, true, false};

    // Start by writing directly from vertices to clipped (no initial copy)
    std::vector<VERTEX>* currentInput = &vertices;
    std::vector<VERTEX>* currentOutput = &clipped;
    std::vector<VERTEX> tempB;

    for (int i = 0; i < 6; ++i) {
        *currentOutput = Clipping(*currentInput, components[i], nears[i]);
        
        if (i < 5) {
            if (currentInput == &vertices) {
                currentInput = &clipped;
                currentOutput = &tempB;
            } else {
                std::swap(currentInput, currentOutput);
            }
        }
    }
}

std::vector<VERTEX> Renderer::Clipping(const std::vector<VERTEX>& vertices, const int component, const bool Near) {
    std::vector<VERTEX> clipped;
    clipped.reserve(vertices.size() * 2); // Clipping can double triangle count

    for (int i = 0; i < static_cast<int>(vertices.size()); i += 3)
    {
        const VERTEX& v0 = vertices[i];
        const VERTEX& v1 = vertices[i + 1];
        const VERTEX& v2 = vertices[i + 2];

        // Classify vertices
        auto isInside = [&](const VERTEX& v) {
            float w = v.W();
            float val = v.data[component];
            return Near ? (val > -w) : (val < w);
        };

        bool in0 = isInside(v0);
        bool in1 = isInside(v1);
        bool in2 = isInside(v2);
        int insideCount = (in0 ? 1 : 0) + (in1 ? 1 : 0) + (in2 ? 1 : 0);

        // All vertices inside - keep triangle as-is
        if (insideCount == 3) {
            clipped.push_back(v0);
            clipped.push_back(v1);
            clipped.push_back(v2);
        }
        // One vertex inside - create 1 smaller triangle
        else if (insideCount == 1) {
            VERTEX insideVert, outsideVert0, outsideVert1;
            
            if (in0) {
                insideVert = v0; outsideVert0 = v1; outsideVert1 = v2;
            } else if (in1) {
                insideVert = v1; outsideVert0 = v2; outsideVert1 = v0;
            } else { // in2
                insideVert = v2; outsideVert0 = v0; outsideVert1 = v1;
            }
            
            VERTEX newVert0 = HomogenousPlaneIntersect(insideVert, outsideVert0, component, Near);
            VERTEX newVert1 = HomogenousPlaneIntersect(insideVert, outsideVert1, component, Near);
            
            // Output triangle with correct winding
            clipped.push_back(insideVert);
            clipped.push_back(newVert0);
            clipped.push_back(newVert1);
        }
        // Two vertices inside - create 2 triangles (quad split)
        else if (insideCount == 2) {
            VERTEX insideVert0, insideVert1, outsideVert;
            
            if (!in0) {
                outsideVert = v0; insideVert0 = v1; insideVert1 = v2;
            } else if (!in1) {
                outsideVert = v1; insideVert0 = v2; insideVert1 = v0;
            } else { // !in2
                outsideVert = v2; insideVert0 = v0; insideVert1 = v1;
            }
            
            VERTEX newVert0 = HomogenousPlaneIntersect(insideVert0, outsideVert, component, Near);
            VERTEX newVert1 = HomogenousPlaneIntersect(insideVert1, outsideVert, component, Near);
            
            // Triangle 1: insideVert0, insideVert1, newVert0
            clipped.push_back(insideVert0);
            clipped.push_back(insideVert1);
            clipped.push_back(newVert0);
            
            // Triangle 2: insideVert1, newVert1, newVert0
            clipped.push_back(insideVert1);
            clipped.push_back(newVert1);
            clipped.push_back(newVert0);
        }
        // else insideCount == 0: triangle completely outside, discard
    }
    
    return clipped;
}

VERTEX Renderer::HomogenousPlaneIntersect(const VERTEX& vert2, const VERTEX& vert1, const int component, const bool Near) {
    // Interpolates on the line between both vertices to get a new point on the line between them
    // v2 is the vertex that is actually visible, v1 is behind the near plane

    VERTEX newVert = vert1;
    VERTEX v = vert1;

    v.SetXYZW(vert1.GetXYZW() - vert2.GetXYZW());

    float i0 = vert1.data[component];
    float w0 = vert1.data[3]; // w is at index 3

    float vi = v.data[component];
    float vw = v.data[3];

    float t;
    if (Near)
        t = (i0 + w0) / (vw + vi); // near clipping
    else
        t = (i0 - w0) / (vi - vw); // far clipping

    for (int i = 0; i < 7; i++)
        newVert.data[i] = glm::mix(vert1.data[i], vert2.data[i], t); // interpolate attributes

    return newVert;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

glm::mat4 Renderer::CalcModelMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size) {
    // position is the bottom right corner if this is a 2D object
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch (X-axis)
	model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw (Y-axis)
	model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z-axis)
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

glm::mat4 Renderer::CalcModelMatrix(const glm::vec3& position, const float rotation, const glm::vec3& size) {
    // position is the bottom right corner if this is a 2D object
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z-axis)
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

CHAR_INFO Renderer::GetCharInfo(const glm::ivec3& rgb) {
    if (!_colorLUTComputed) {
        PrecomputeColorLUT();
    }

    // Pre-computed _rgbLUTDepth squared for faster indexing
    const int index = (rgb.r * _rgbLUTDepth * _rgbLUTDepth) + (rgb.g * _rgbLUTDepth) + rgb.b;
    return _colorLUT[index];
}

void Renderer::SetAntialiasingsamples(int samples) {
    // Clamp to reasonable values (1 to 16 samples)
    _antialiasing_samples = std::max(1, std::min(16, samples));
}

int Renderer::GetAntialiasingsamples() const {
    return _antialiasing_samples;
}

void Renderer::SetWireframe(bool wireframe) {
    _wireframe = wireframe;
}

bool Renderer::GetWireframe() const {
    return _wireframe;
}

void Renderer::SetBackfaceCulling(bool backfaceCulling) {
    _backface_culling = backfaceCulling;
}

bool Renderer::GetBackfaceCulling() const {
    return _backface_culling;
}

void Renderer::SetCCW(bool ccw) {
    _ccw = ccw;
}

bool Renderer::GetCCW() const {
    return _ccw;
}

void Renderer::SetAntialiasing(bool antialiasing) {
    _antialiasing = antialiasing;
}

bool Renderer::GetAntialiasing() const {
    return _antialiasing;
}

void Renderer::SetContrast(const float contrast) {
    _contrast = std::min(5.0f, std::max(0.0f, contrast));
    _colorLUTComputed = false; // Mark LUT for recomputation
}

float Renderer::GetContrast() const {
    return _contrast;
}

void Renderer::GenerateSubPixelOffsets(int sampleCount) {
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

const std::vector<std::pair<float, float>>& Renderer::GetSubpixelOffsets() {
    if (_subpixel_offsets.empty()) {
        GenerateSubPixelOffsets(_antialiasing_samples);
    }
    return _subpixel_offsets;
}

void Renderer::TestRenderFont() {
    Screen& screen = Screen::GetInst();
    int screenWidth = screen.GetWidth();
    int screenHeight = screen.GetHeight();
    
    // Get the number of characters in the ramp
    const int numChars = static_cast<int>(_charRamp.size());
    
    // Calculate grid dimensions (square-ish grid)
    int gridCols = static_cast<int>(std::ceil(std::sqrt(numChars)));
    int gridRows = static_cast<int>(std::ceil(static_cast<float>(numChars) / gridCols));
    
    // Calculate patch size for each character
    int patchWidth = screenWidth / gridCols;
    int patchHeight = screenHeight / gridRows;
    
    // Combined foreground and background color
    Palette& palette = screen.GetPalette();
    unsigned short combinedColor = palette.GetFgColor(6) | palette.GetBgColor(11);
    
    // Fill screen with character patches
    for (int charIdx = 0; charIdx < numChars; ++charIdx) {
        // Calculate which patch this character belongs to
        int patchX = charIdx % gridCols;
        int patchY = charIdx / gridCols;
        
        // Calculate the bounds of this patch
        int startX = patchX * patchWidth;
        int startY = patchY * patchHeight;
        int endX = (patchX == gridCols - 1) ? screenWidth : startX + patchWidth;  // Handle remainder
        int endY = (patchY == gridRows - 1) ? screenHeight : startY + patchHeight; // Handle remainder
        
        // Fill the entire patch with this character
        wchar_t currentChar = _charRamp[charIdx];
        CHAR_INFO charInfo;

        charInfo.Char.UnicodeChar = currentChar;
        charInfo.Attributes = combinedColor;
        
        for (int y = startY; y < endY; ++y) {
            for (int x = startX; x < endX; ++x) {
                screen.PlotPixel(glm::vec2(x, y), charInfo);
            }
        }
    }
}

void Renderer::PrecomputeColorLUT() {
    Palette& palette = Screen::GetInst().GetPalette();
    if (_colorLUTComputed) return;
    
    constexpr float inv16 = 1.0f / 16.0f;
    
    // Precompute all possible RGB combinations
    for (int r = 0; r < _rgbLUTDepth; ++r) {
        for (int g = 0; g < _rgbLUTDepth; ++g) {
            for (int b = 0; b < _rgbLUTDepth; ++b) {
                // Convert discrete RGB to normalized [0,1] range for contrast adjustment
                glm::vec3 rgb(
                    r / float(_rgbLUTDepth - 1),
                    g / float(_rgbLUTDepth - 1),
                    b / float(_rgbLUTDepth - 1)
                );

                // Apply contrast adjustment
                rgb = (rgb - 0.5f) * _contrast + 0.5f;
                rgb = glm::clamp(rgb, 0.0f, 1.0f);

                // Find best match using original algorithm
                float minError = FLT_MAX;
                int bestFgIndex = 0, bestBgIndex = 0, bestCharIndex = 0;

                for (int fgIdx = 0; fgIdx < 16; ++fgIdx) {
                    for (int bgIdx = 0; bgIdx < 16; ++bgIdx) {
                        for (int charIdx = 0; charIdx < _charRamp.size(); ++charIdx) {
                            float coverage = _charCoverage[charIdx];
                            
                            // Convert palette colors from 0-15 to 0.0-1.0 for comparison
                            glm::vec3 fgColor = glm::vec3(palette.GetRGB(fgIdx)) * inv16;
                            glm::vec3 bgColor = glm::vec3(palette.GetRGB(bgIdx)) * inv16;
                            
                            glm::vec3 simulatedColor = coverage * fgColor + (1.0f - coverage) * bgColor;
                            glm::vec3 diff = rgb - simulatedColor;
                            float error = glm::dot(diff, diff);

                            if (error < minError) {
                                minError = error;
                                bestFgIndex = fgIdx;
                                bestBgIndex = bgIdx;
                                bestCharIndex = charIdx;
                            }
                        }
                    }
                }

                // Store precomputed result
                int index = (r * _rgbLUTDepth * _rgbLUTDepth) + (g * _rgbLUTDepth) + b;
                wchar_t glyph = _charRamp[bestCharIndex];
                unsigned short fgColor = static_cast<unsigned short>(palette.GetFgColor(bestFgIndex));
                unsigned short bgColor = static_cast<unsigned short>(palette.GetBgColor(bestBgIndex));
                unsigned short combinedColor = fgColor | bgColor;

                _colorLUT[index] = {glyph, combinedColor};
            }
        }
    }

    _colorLUTComputed = true;
}

void Renderer::TestRenderColorDiscrete() {
    Screen& screen = Screen::GetInst();
    int screenWidth = screen.GetWidth();
    int screenHeight = screen.GetHeight();

    // 16 colors, arrange in a 4x4 grid
    const int numColors = Palette::COLOR_COUNT;
    const int gridCols = 4;
    const int gridRows = 4;

    int patchWidth = screenWidth / gridCols;
    int patchHeight = screenHeight / gridRows;

    Palette& palette = screen.GetPalette();

    for (int colorIdx = 0; colorIdx < numColors; ++colorIdx) {
        int patchX = colorIdx % gridCols;
        int patchY = colorIdx / gridCols;

        int startX = patchX * patchWidth;
        int startY = patchY * patchHeight;
        int endX = (patchX == gridCols - 1) ? screenWidth : startX + patchWidth;
        int endY = (patchY == gridRows - 1) ? screenHeight : startY + patchHeight;

        // Use a solid block character for visibility
        CHAR_INFO charInfo;
        charInfo.Char.UnicodeChar = 'B';
        charInfo.Attributes = palette.GetFgColor(colorIdx) | palette.GetBgColor(colorIdx);
        std::wstringstream ss;
        ss << L"Color " << colorIdx << L" RGB: ("
           << palette.GetRGB(colorIdx).r << L", "
           << palette.GetRGB(colorIdx).g << L", "
           << palette.GetRGB(colorIdx).b << L") "
           << "FG: " << palette.GetFgColor(colorIdx) << L" | BG: " << palette.GetBgColor(colorIdx);
        Logger::Debug(ss.str());

        for (int y = startY; y < endY; ++y) {
            for (int x = startX; x < endX; ++x) {
                screen.PlotPixel(glm::vec2(x, y), charInfo);
            }
        }
    }
}

void Renderer::ClearBuffers() {
    std::fill(_depth_buffer.begin(), _depth_buffer.end(), -FLT_MAX);
    std::fill(_color_buffer.begin(), _color_buffer.end(), glm::ivec4(_background_col, 1));
}

glm::ivec3 Renderer::GetBackgroundCol() const {
    return _background_col;
}

void Renderer::SetBackgroundCol(const glm::ivec3& col) {
    _background_col = glm::clamp(col, glm::ivec3(0), glm::ivec3(15));
}

void Renderer::OverwritePxBuffWithColBuff() {
    LogDiagnostics();
    ResetDiagnostics();

    auto& screen = Screen::GetInst();
    auto& pixelBuffer = screen.GetPixelBuffer();
    const size_t bufferSize = _color_buffer.size();
    
    if (!_colorLUTComputed) {
        PrecomputeColorLUT();
    }
    
    const size_t unrollFactor = 4;
    const size_t unrolledEnd = (bufferSize / unrollFactor) * unrollFactor;
    
    size_t i = 0;
    for (; i < unrolledEnd; i += unrollFactor) {
        const auto& color0 = _color_buffer[i];
        const auto& color1 = _color_buffer[i + 1];
        const auto& color2 = _color_buffer[i + 2];
        const auto& color3 = _color_buffer[i + 3];
        

        const int index0 = (color0.r * _rgbLUTDepth * _rgbLUTDepth) + (color0.g * _rgbLUTDepth) + color0.b;
        const int index1 = (color1.r * _rgbLUTDepth * _rgbLUTDepth) + (color1.g * _rgbLUTDepth) + color1.b;
        const int index2 = (color2.r * _rgbLUTDepth * _rgbLUTDepth) + (color2.g * _rgbLUTDepth) + color2.b;
        const int index3 = (color3.r * _rgbLUTDepth * _rgbLUTDepth) + (color3.g * _rgbLUTDepth) + color3.b;
        
        pixelBuffer[i] = _colorLUT[index0];
        pixelBuffer[i + 1] = _colorLUT[index1];
        pixelBuffer[i + 2] = _colorLUT[index2];
        pixelBuffer[i + 3] = _colorLUT[index3];
    }
    
    for (; i < bufferSize; ++i) {
        const auto& color = _color_buffer[i];
        const int index = (color.r * _rgbLUTDepth * _rgbLUTDepth) + (color.g * _rgbLUTDepth) + color.b;
        pixelBuffer[i] = _colorLUT[index];
    }
}

void Renderer::PlotColor(int x, int y, const glm::ivec3& color) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    _color_buffer[y * screenWidth + x] = glm::ivec4(color, 1);
}

void Renderer::PlotColor(int x, int y, const glm::ivec4& color) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    _color_buffer[y * screenWidth + x] = color;
}

void Renderer::PlotColor(int x, int y, const glm::ivec4& color, float depth) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    const int idx = y * screenWidth + x;
    if (depth > _depth_buffer[idx]) {
        _color_buffer[idx] = color;
        _depth_buffer[idx] = depth;
    }
}

void Renderer::PlotColorBlend(int x, int y, const glm::ivec4& color, float depth) {
    const int screenWidth = Screen::GetInst().GetWidth();
    const int screenHeight = Screen::GetInst().GetHeight();
    
    if (x < 0 || x >= screenWidth || y < 0 || y >= screenHeight) {
        return;
    }

    const int idx = y * screenWidth + x;
    
    // for now since we don't have depth ordering, it just assumes everything is opaque
    if (depth > _depth_buffer[idx]) {
        _color_buffer[idx] = color;
        _depth_buffer[idx] = depth;
    }
}

void Renderer::PerspectiveAndViewportTransform(std::vector<VERTEX>& vertices) {
    // **OPTIMIZATION 1**: Cache screen dimensions once (expensive getter calls)
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

void Renderer::ResetDiagnostics() {
    _triangles_inputted = 0;
    _triangles_past_clipping = 0;
    _triangles_past_backface_culling = 0;
}

void Renderer::LogDiagnostics() const {
    Logger::Info("Renderer Diagnostics:");
    Logger::Info("  Triangles Inputted: " + std::to_string(_triangles_inputted));
    Logger::Info("  Triangles Past Clipping: " + std::to_string(_triangles_past_clipping));
    Logger::Info("  Triangles Past Backface Culling: " + std::to_string(_triangles_past_backface_culling));
}