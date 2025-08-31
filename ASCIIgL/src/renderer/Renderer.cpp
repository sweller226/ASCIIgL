#include <ASCIIgL/renderer/Renderer.hpp>

#include <execution>
#include <numeric>

#include <ASCIIgL/engine/Logger.hpp>
#include <ASCIIgL/engine/Collision.hpp>

#include <ASCIIgL/util/MathUtil.hpp>

#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/renderer/RenderEnums.hpp>

// =============================================================================
// PUBLIC HIGH-LEVEL DRAWING FUNCTIONS
// =============================================================================

void Renderer::DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh) {
    for (int i = 0; i < mesh->textures.size(); i++) {
        if (mesh->textures[i]->texType == "texture_diffuse") {
            RenderTriangles(VSHADER, mesh->vertices, mesh->textures[i]);
        }
    }
}

void Renderer::DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3 position, const glm::vec2 rotation, const glm::vec3 size, const Camera3D& camera) {
    glm::mat4 model = CalcModelMatrix(position, rotation, size);

    VSHADER.SetModel(model);
    VSHADER.SetView(camera.view);
    VSHADER.SetProj(camera.proj);

    for (int i = 0; i < mesh->textures.size(); i++) {
        if (mesh->textures[i]->texType == "texture_diffuse") {
            RenderTriangles(VSHADER, mesh->vertices, mesh->textures[i]);
        }
    }
}

void Renderer::DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3 position, const glm::vec2 rotation, const glm::vec3 size, const Camera3D& camera) {
    glm::mat4 model = CalcModelMatrix(position, rotation, size);

    VSHADER.SetModel(model);
    VSHADER.SetView(camera.view);
    VSHADER.SetProj(camera.proj);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(VSHADER, ModelObj.meshes[i]);
    }
}

void Renderer::DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4 model, const Camera3D& camera)
{
    VSHADER.SetModel(model);
    VSHADER.SetView(camera.view);
    VSHADER.SetProj(camera.proj);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(VSHADER, ModelObj.meshes[i]);
    }
}

void Renderer::Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2 position, const glm::vec2 rotation, const glm::vec2 size, const Camera2D& camera, int layer)
{
    glm::mat4 model = CalcModelMatrix(glm::vec3(position, layer), rotation, glm::vec3(size, 0.0f));

    VSHADER.SetModel(model);
    VSHADER.SetView(camera.view);
    VSHADER.SetProj(camera.proj);

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

void Renderer::Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2 positionPerc, const glm::vec2 rotation, const glm::vec2 sizePerc, const Camera2D& camera, int layer)
{
    // Convert percentage coordinates to pixel coordinates
    float screenWidth = static_cast<float>(Screen::GetWidth());
    float screenHeight = static_cast<float>(Screen::GetHeight());
    
    glm::vec2 pixelPosition = glm::vec2(positionPerc.x * screenWidth, positionPerc.y * screenHeight);
    glm::vec2 pixelSize = glm::vec2(sizePerc.x * screenWidth, sizePerc.y * screenHeight);
    
    glm::mat4 model = CalcModelMatrix(glm::vec3(pixelPosition, layer), rotation, glm::vec3(pixelSize, 0.0f));

    VSHADER.SetModel(model);
    VSHADER.SetView(camera.view);
    VSHADER.SetProj(camera.proj);

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

void Renderer::DrawScreenBorder(const short col) {
	// DRAWING BORDERS
	DrawLine(1, 1, Screen::GetInstance().GetWidth() - 1, 1, PIXEL_FULL, col);
	DrawLine(Screen::GetInstance().GetWidth() - 1, 1, Screen::GetInstance().GetWidth() - 1, Screen::GetInstance().GetHeight() - 1, PIXEL_FULL, col);
	DrawLine(Screen::GetInstance().GetWidth() - 1, Screen::GetInstance().GetHeight() - 1, 1, Screen::GetInstance().GetHeight() - 1, PIXEL_FULL, col);
	DrawLine(1, 1, 1, Screen::GetInstance().GetHeight() - 1, PIXEL_FULL, col);
}

// =============================================================================
// CORE RENDERING PIPELINE
// =============================================================================

void Renderer::RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex) {
    if (vertices.size() < 3 || vertices.size() % 3 != 0) {
        return;
    }

    // Optimize: Use pre-allocated buffers instead of creating new vectors each frame
    _vertexBuffer.clear();
    _vertexBuffer.reserve(vertices.size()); // Reserve to avoid reallocations
    _vertexBuffer = vertices; // Still need a copy for transformations, but reuse buffer

    // Optimize: Parallel vertex shader execution for better CPU utilization
    std::for_each(std::execution::par_unseq, _vertexBuffer.begin(), _vertexBuffer.end(), 
        [&VSHADER](VERTEX& v) { VSHADER.GLUse(v); });

    _clippedBuffer.clear();
    ClippingHelper(_vertexBuffer, _clippedBuffer);

    // Optimize: Parallel perspective division and viewport transform
    std::for_each(std::execution::par_unseq, _clippedBuffer.begin(), _clippedBuffer.end(), 
        [](VERTEX& v) {
            PerspectiveDivision(v);
            ViewPortTransform(v);
        });

    // Remove back-face culled triangles before rasterization
    if (_backface_culling) {
        _rasterBuffer.clear();
        BackFaceCullHelper(_clippedBuffer, _rasterBuffer);
    } else {
        _rasterBuffer = std::move(_clippedBuffer); // Move semantics to avoid copy
    }

    // Optimize: Initialize tiles only once, then reuse the structure
    if (!_tilesInitialized) {
        InitializeTilesOnce();
        _tilesInitialized = true;
    } else {
        ClearTileTriangleLists();
    }

    BinTrianglesToTiles(_rasterBuffer);

    // Parallel tile rendering (already optimized)
    std::for_each(std::execution::par, _tileBuffer.begin(), _tileBuffer.end(), [&](Tile& tile) {
        if (_wireframe || tex == nullptr) {
            // DrawTileWireframe(tile, _rasterBuffer);
        } else {
            DrawTileTextured(tile, _rasterBuffer, tex);
        }
    });
}

void Renderer::BackFaceCullHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& raster_triangles) {
    if (vertices.size() < 3) return;

    const size_t triangleCount = vertices.size() / 3;
    
    // Create a parallel boolean array to mark which triangles to keep
    std::vector<bool> keepTriangle(triangleCount);
    
    // Create index vector for parallel processing
    std::vector<size_t> triangleIndices(triangleCount);
    std::iota(triangleIndices.begin(), triangleIndices.end(), 0);
    
    // Parallel pass: determine which triangles to keep (no data races)
    std::for_each(std::execution::par_unseq, 
                  triangleIndices.begin(), 
                  triangleIndices.end(),
                  [&](size_t triIndex) {
                      const size_t vertexIndex = triIndex * 3;
                      keepTriangle[triIndex] = !BackFaceCull(
                          vertices[vertexIndex], 
                          vertices[vertexIndex + 1], 
                          vertices[vertexIndex + 2], 
                          _ccw
                      );
                  });
    
    // Sequential pass: collect kept triangles (avoids race conditions on push_back)
    // Reserve space to minimize allocations
    size_t keepCount = std::count(keepTriangle.begin(), keepTriangle.end(), true);
    raster_triangles.clear();
    raster_triangles.reserve(keepCount * 3);
    
    for (size_t triIndex = 0; triIndex < triangleCount; ++triIndex) {
        if (keepTriangle[triIndex]) {
            const size_t vertexIndex = triIndex * 3;
            raster_triangles.push_back(vertices[vertexIndex]);
            raster_triangles.push_back(vertices[vertexIndex + 1]);
            raster_triangles.push_back(vertices[vertexIndex + 2]);
        }
    }
}

// =============================================================================
// TILE-BASED RENDERING FUNCTIONS
// =============================================================================

void Renderer::InitializeTilesOnce() {
    // One-time initialization of tile structure - much faster than recreating every frame
    int tileCount = Screen::GetTileCountX() * Screen::GetTileCountY();
    _tileBuffer.clear();
    _tileBuffer.resize(tileCount);
    
    for (int ty = 0; ty < Screen::GetTileCountY(); ++ty) {
        for (int tx = 0; tx < Screen::GetTileCountX(); ++tx) {
            int tileIndex = ty * Screen::GetTileCountX() + tx;
            int posX = tx * Screen::GetTileSizeX();
            int posY = ty * Screen::GetTileSizeY();

            // Clamp tile size if it would overflow the screen
            int sizeX = std::min(Screen::GetTileSizeX(), Screen::GetWidth() - posX);
            int sizeY = std::min(Screen::GetTileSizeY(), Screen::GetHeight() - posY);

            _tileBuffer[tileIndex].position = glm::ivec2(posX, posY);
            _tileBuffer[tileIndex].size = glm::ivec2(sizeX, sizeY);
            
            // Pre-reserve space to avoid frequent reallocations
            _tileBuffer[tileIndex].tri_indices_encapsulated.reserve(64);
            _tileBuffer[tileIndex].tri_indices_partial.reserve(64);
        }
    }
}

void Renderer::ClearTileTriangleLists() {
    // Much faster than recreating tiles - just clear the triangle lists
    for (auto& tile : _tileBuffer) {
        tile.tri_indices_encapsulated.clear();
        tile.tri_indices_partial.clear();
    }
}

void Renderer::InitializeTiles(std::vector<Tile>& tiles) {
    for (int ty = 0; ty < Screen::GetTileCountY(); ++ty) {
        for (int tx = 0; tx < Screen::GetTileCountX(); ++tx) {
            int tileIndex = ty * Screen::GetTileCountX() + tx;
            int posX = tx * Screen::GetTileSizeX();
            int posY = ty * Screen::GetTileSizeY();

            // Clamp tile size if it would overflow the screen
            int sizeX = std::min(Screen::GetTileSizeX(), Screen::GetWidth() - posX);
            int sizeY = std::min(Screen::GetTileSizeY(), Screen::GetHeight() - posY);

            tiles[tileIndex].position = glm::ivec2(posX, posY);
            tiles[tileIndex].size = glm::ivec2(sizeX, sizeY);
        }
    }
}

void Renderer::BinTrianglesToTiles(const std::vector<VERTEX>& raster_triangles) {
    // Optimized version that works directly with pre-allocated tile buffer
    const int tileSizeX = Screen::GetTileSizeX();
    const int tileSizeY = Screen::GetTileSizeY();
    const int tileCountX = Screen::GetTileCountX();
    const int tileCountY = Screen::GetTileCountY();
    const float invTileSizeX = 1.0f / tileSizeX; // Avoid divisions in hot loop
    const float invTileSizeY = 1.0f / tileSizeY;

    for (int i = 0; i < static_cast<int>(raster_triangles.size()); i += 3) {
        const auto [minTriPt, maxTriPt] = MathUtil::ComputeBoundingBox(
            raster_triangles[i].GetXY(),
            raster_triangles[i + 1].GetXY(),
            raster_triangles[i + 2].GetXY()
        );

        // Optimized tile range calculation using pre-computed inverse
        int minTileX = std::max(0, static_cast<int>(minTriPt.x * invTileSizeX));
        int maxTileX = std::min(tileCountX - 1, static_cast<int>(maxTriPt.x * invTileSizeX));
        int minTileY = std::max(0, static_cast<int>(minTriPt.y * invTileSizeY));
        int maxTileY = std::min(tileCountY - 1, static_cast<int>(maxTriPt.y * invTileSizeY));

        // Cache triangle vertices for encapsulation test
        const VERTEX& v1 = raster_triangles[i];
        const VERTEX& v2 = raster_triangles[i + 1];
        const VERTEX& v3 = raster_triangles[i + 2];

        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            const int rowOffset = ty * tileCountX;
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                const int tileIndex = rowOffset + tx;
                Tile& tile = _tileBuffer[tileIndex];

                // Check if tile fully encapsulates the triangle
                if (DoesTileEncapsulate(tile, v1, v2, v3)) {
                    tile.tri_indices_encapsulated.push_back(i);
                } else {
                    tile.tri_indices_partial.push_back(i);
                }
            }
        }
    }
}

bool Renderer::DoesTileEncapsulate(const Tile& tile, const VERTEX& v1, const VERTEX& v2, const VERTEX& v3)
{
    // Get triangle bounding box
    const auto [minTriPt, maxTriPt] = MathUtil::ComputeBoundingBox(
        v1.GetXY(),
        v2.GetXY(),
        v3.GetXY()
    );

    // Get tile bounding box
    glm::vec2 tileMin = tile.position;
    glm::vec2 tileMax = tile.position + tile.size;

    // Check if triangle bounding box is fully inside tile bounding box
    bool fullyInside =
        (minTriPt.x >= tileMin.x) && (maxTriPt.x <= tileMax.x) &&
        (minTriPt.y >= tileMin.y) && (maxTriPt.y <= tileMax.y);

    return fullyInside;
}

void Renderer::DrawTileTextured(const Tile& tile, const std::vector<VERTEX>& raster_triangles, const Texture* tex) {
    // Optimize: Separate loops by antialiasing mode to reduce branch prediction misses
    if (_antialiasing) {
        // Antialiased path - process all encapsulated triangles first
        for (int triIndex : tile.tri_indices_encapsulated) {
            DrawTriangleTexturedAntialiased(raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
        }
        
        // Then process partial triangles
        for (int triIndex : tile.tri_indices_partial) {
            DrawTriangleTexturedPartialAntialiased(tile, raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
        }
    } else {
        // Non-antialiased path - process all encapsulated triangles first
        for (int triIndex : tile.tri_indices_encapsulated) {
            DrawTriangleTextured(raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
        }
        
        // Then process partial triangles
        for (int triIndex : tile.tri_indices_partial) {
            DrawTriangleTexturedPartial(tile, raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], tex);
        }
    }
}

// =============================================================================
// TRIANGLE RASTERIZATION FUNCTIONS
// =============================================================================

void Renderer::DrawTriangleTextured(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
    const int texWidth = tex->GetWidth();
    const int texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0)
    {
        Logger::Error("Invalid texture size. Width: " + std::to_string(texWidth) + ", Height: " + std::to_string(texHeight));
        return;
    }

    // Cache frequently used values to avoid repeated function calls
    const int screenWidth = Screen::GetWidth();
    const int screenHeight = Screen::GetHeight();
    float* const depthBuffer = Screen::GetDepthBuffer();

    int x1 = vert1.X(), x2 = vert2.X(), x3 = vert3.X();
    int y1 = vert1.Y(), y2 = vert2.Y(), y3 = vert3.Y();
    float w1 = vert1.UVW(), w2 = vert2.UVW(), w3 = vert3.UVW();
    float u1 = vert1.U(), u2 = vert2.U(), u3 = vert3.U();
    float v1 = vert1.V(), v2 = vert2.V(), v3 = vert3.V();

    // Sort vertices by y
    if (y2 < y1) { std::swap(y1, y2); std::swap(x1, x2); std::swap(u1, u2); std::swap(v1, v2); std::swap(w1, w2); }
    if (y3 < y1) { std::swap(y1, y3); std::swap(x1, x3); std::swap(u1, u3); std::swap(v1, v3); std::swap(w1, w3); }
    if (y3 < y2) { std::swap(y2, y3); std::swap(x2, x3); std::swap(u2, u3); std::swap(v2, v3); std::swap(w2, w3); }

    // Helper lambda for drawing scanlines
    auto drawScanlines = [&](int startY, int endY, int xa, int ya, int xb, int yb, 
                             float ua, float va, float wa, float ub, float vb, float wb,
                             int x_long, int y_long, float u_long, float v_long, float w_long) {
        int dy_short = endY - startY;
        int dy_long = y_long - y1;
        int dx_short = xb - xa;
        int dx_long = x_long - x1;
        float du_short = ub - ua, dv_short = vb - va, dw_short = wb - wa;
        float du_long = u_long - u1, dv_long = v_long - v1, dw_long = w_long - w1;

        float dax_step = dy_short ? dx_short / (float)abs(dy_short) : 0;
        float dbx_step = dy_long ? dx_long / (float)abs(dy_long) : 0;
        float du1_step = dy_short ? du_short / (float)abs(dy_short) : 0;
        float dv1_step = dy_short ? dv_short / (float)abs(dy_short) : 0;
        float dw1_step = dy_short ? dw_short / (float)abs(dy_short) : 0;
        float du2_step = dy_long ? du_long / (float)abs(dy_long) : 0;
        float dv2_step = dy_long ? dv_long / (float)abs(dy_long) : 0;
        float dw2_step = dy_long ? dw_long / (float)abs(dy_long) : 0;

        if (dy_short) {
            for (int i = startY; i <= endY && i < screenHeight; i++) {
                int ax = xa + (float)(i - startY) * dax_step;
                int bx = x1 + (float)(i - y1) * dbx_step;
                float tex_su = ua + (float)(i - startY) * du1_step;
                float tex_sv = va + (float)(i - startY) * dv1_step;
                float tex_sw = wa + (float)(i - startY) * dw1_step;
                float tex_eu = u1 + (float)(i - y1) * du2_step;
                float tex_ev = v1 + (float)(i - y1) * dv2_step;
                float tex_ew = w1 + (float)(i - y1) * dw2_step;
                if (ax > bx) { std::swap(ax, bx); std::swap(tex_su, tex_eu); std::swap(tex_sv, tex_ev); std::swap(tex_sw, tex_ew); }
                
                float tstep = (bx != ax) ? 1.0f / ((float)(bx - ax)) : 0.0f;
                float t = 0.0f;
                
                // Pre-compute buffer row offset for this scanline
                const int bufferRowOffset = i * screenWidth;
                
                for (int j = ax; j < bx && j < screenWidth; j++) {
                    float tex_w = (1.0f - t) * tex_sw + t * tex_ew;
                    const int bufferIndex = bufferRowOffset + j;
                    
                    if (tex_w > depthBuffer[bufferIndex]) {
                        float tex_uw = ((1.0f - t) * tex_su + t * tex_eu) / tex_w;
                        float tex_vw = ((1.0f - t) * tex_sv + t * tex_ev) / tex_w;
                        
                        if (tex_uw < 1.0f && tex_vw < 1.0f) {
                            // Use integer texture coordinates for better cache performance
                            float texWidthProd = tex_uw * texWidth;
                            float texHeightProd = tex_vw * texHeight;
                            float blendedGrayScale = tex->GetPixelCol(texWidthProd, texHeightProd);
                            Screen::PlotPixel(glm::vec2(j, i), GetColGlyph(blendedGrayScale));
                            depthBuffer[bufferIndex] = tex_w;
                        }
                    }
                    t += tstep;
                }
            }
        }
    };

    // Upper half: y1 to y2
    drawScanlines(y1, y2, x1, y1, x2, y2, u1, v1, w1, u2, v2, w2, x3, y3, u3, v3, w3);
    
    // Lower half: y2 to y3
    drawScanlines(y2, y3, x2, y2, x3, y3, u2, v2, w2, u3, v3, w3, x3, y3, u3, v3, w3);
}

void Renderer::DrawTriangleTexturedPartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
    const int texWidth = tex->GetWidth();
    const int texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0) {
        Logger::Error("Invalid texture size. Width: " + std::to_string(texWidth) + ", Height: " + std::to_string(texHeight));
        return;
    }

    // Cache frequently used values
    const int screenWidth = Screen::GetWidth();
    const int screenHeight = Screen::GetHeight();
    float* const depthBuffer = Screen::GetDepthBuffer();

    int x1 = vert1.X(), x2 = vert2.X(), x3 = vert3.X();
    int y1 = vert1.Y(), y2 = vert2.Y(), y3 = vert3.Y();
    float w1 = vert1.UVW(), w2 = vert2.UVW(), w3 = vert3.UVW();
    float u1 = vert1.U(), u2 = vert2.U(), u3 = vert3.U();
    float v1 = vert1.V(), v2 = vert2.V(), v3 = vert3.V();

    // Sort vertices by y
    if (y2 < y1) { std::swap(y1, y2); std::swap(x1, x2); std::swap(u1, u2); std::swap(v1, v2); std::swap(w1, w2); }
    if (y3 < y1) { std::swap(y1, y3); std::swap(x1, x3); std::swap(u1, u3); std::swap(v1, v3); std::swap(w1, w3); }
    if (y3 < y2) { std::swap(y2, y3); std::swap(x2, x3); std::swap(u2, u3); std::swap(v2, v3); std::swap(w2, w3); }

    // Tile bounds - cache these values
    const int minX = int(tile.position.x);
    const int maxX = int(tile.position.x + tile.size.x);
    const int minY = int(tile.position.y);
    const int maxY = int(tile.position.y + tile.size.y);

    // Helper lambda for drawing scanlines
    auto drawScanlines = [&](int startY, int endY, int xa, int ya, int xb, int yb, 
                             float ua, float va, float wa, float ub, float vb, float wb,
                             int x_long, int y_long, float u_long, float v_long, float w_long) {
        int dy_short = endY - startY;
        int dy_long = y_long - y1;
        int dx_short = xb - xa;
        int dx_long = x_long - x1;
        float du_short = ub - ua, dv_short = vb - va, dw_short = wb - wa;
        float du_long = u_long - u1, dv_long = v_long - v1, dw_long = w_long - w1;

        float dax_step = dy_short ? dx_short / (float)abs(dy_short) : 0;
        float dbx_step = dy_long ? dx_long / (float)abs(dy_long) : 0;
        float du1_step = dy_short ? du_short / (float)abs(dy_short) : 0;
        float dv1_step = dy_short ? dv_short / (float)abs(dy_short) : 0;
        float dw1_step = dy_short ? dw_short / (float)abs(dy_short) : 0;
        float du2_step = dy_long ? du_long / (float)abs(dy_long) : 0;
        float dv2_step = dy_long ? dv_long / (float)abs(dy_long) : 0;
        float dw2_step = dy_long ? dw_long / (float)abs(dy_long) : 0;

        if (dy_short) {
            for (int i = std::max(startY, minY); i <= std::min(endY, maxY - 1) && i < screenHeight; i++) {
                int ax = xa + (float)(i - startY) * dax_step;
                int bx = x1 + (float)(i - y1) * dbx_step;
                float tex_su = ua + (float)(i - startY) * du1_step;
                float tex_sv = va + (float)(i - startY) * dv1_step;
                float tex_sw = wa + (float)(i - startY) * dw1_step;
                float tex_eu = u1 + (float)(i - y1) * du2_step;
                float tex_ev = v1 + (float)(i - y1) * dv2_step;
                float tex_ew = w1 + (float)(i - y1) * dw2_step;
                if (ax > bx) { std::swap(ax, bx); std::swap(tex_su, tex_eu); std::swap(tex_sv, tex_ev); std::swap(tex_sw, tex_ew); }
                
                float tstep = (bx != ax) ? 1.0f / ((float)(bx - ax)) : 0.0f;
                int startX = std::max(ax, minX);
                int endX = std::min(bx, maxX);
                
                // Calculate starting t value based on clipped X position
                float t = (startX - ax) * tstep;
                
                // Pre-compute buffer row offset
                const int bufferRowOffset = i * screenWidth;
                
                for (int j = startX; j < endX && j < screenWidth; j++) {
                    float tex_w = (1.0f - t) * tex_sw + t * tex_ew;
                    const int bufferIndex = bufferRowOffset + j;
                    
                    if (tex_w > depthBuffer[bufferIndex]) {
                        float tex_uw = ((1.0f - t) * tex_su + t * tex_eu) / tex_w;
                        float tex_vw = ((1.0f - t) * tex_sv + t * tex_ev) / tex_w;
                        
                        if (tex_uw < 1.0f && tex_vw < 1.0f) {
                            float texWidthProd = tex_uw * texWidth;
                            float texHeightProd = tex_vw * texHeight;
                            float blendedGrayScale = tex->GetPixelCol(texWidthProd, texHeightProd);
                            Screen::PlotPixel(glm::vec2(j, i), GetColGlyph(blendedGrayScale));
                            depthBuffer[bufferIndex] = tex_w;
                        }
                    }
                    t += tstep;
                }
            }
        }
    };
    
    // Upper half: y1 to y2
    drawScanlines(y1, y2, x1, y1, x2, y2, u1, v1, w1, u2, v2, w2, x3, y3, u3, v3, w3);
    
    // Lower half: y2 to y3
    drawScanlines(y2, y3, x2, y2, x3, y3, u2, v2, w2, u3, v3, w3, x3, y3, u3, v3, w3);
}

void Renderer::DrawTriangleTexturedPartialAntialiased(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
    int texWidth = tex->GetWidth();
    int texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0) {
        return;
    }

    // Get triangle bounding box clipped to tile
    int minX = std::max((int)tile.position.x, (int)std::min({vert1.X(), vert2.X(), vert3.X()}));
    int maxX = std::min((int)(tile.position.x + tile.size.x) - 1, (int)std::max({vert1.X(), vert2.X(), vert3.X()}));
    int minY = std::max((int)tile.position.y, (int)std::min({vert1.Y(), vert2.Y(), vert3.Y()}));
    int maxY = std::min((int)(tile.position.y + tile.size.y) - 1, (int)std::max({vert1.Y(), vert2.Y(), vert3.Y()}));

    // Clamp to screen bounds as well
    minX = std::max(0, minX);
    maxX = std::min((int)Screen::GetInstance().GetWidth() - 1, maxX);
    minY = std::max(0, minY);
    maxY = std::min((int)Screen::GetInstance().GetHeight() - 1, maxY);

    // Pre-compute triangle vertex positions and edge deltas (moved outside loops)
    const float ax = vert1.X(), ay = vert1.Y();
    const float bx = vert2.X(), by = vert2.Y();
    const float cx = vert3.X(), cy = vert3.Y();
    
    // Edge vectors for edge functions
    const float ab_dx = bx - ax, ab_dy = by - ay;
    const float bc_dx = cx - bx, bc_dy = cy - by;
    const float ca_dx = ax - cx, ca_dy = ay - cy;
    
    // Pre-compute triangle area and inverse (for barycentric coordinates)
    const float totalArea = abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay));
    if (totalArea <= 1e-7f) return; // Degenerate triangle
    const float invTotalArea = 1.0f / totalArea;
    
    // Pre-compute vertex attributes
    const float vert1_uvw = vert1.UVW(), vert2_uvw = vert2.UVW(), vert3_uvw = vert3.UVW();
    const float vert1_u = vert1.U(), vert2_u = vert2.U(), vert3_u = vert3.U();
    const float vert1_v = vert1.V(), vert2_v = vert2.V(), vert3_v = vert3.V();
    
    // Cache screen dimensions and depth buffer reference
    const int screenWidth = Screen::GetWidth();
    float* depthBuffer = Screen::GetDepthBuffer();
    
    // Get dynamic sub-pixel sampling pattern for anti-aliasing
    const auto subPixelOffsets = GenerateSubPixelOffsets(_antialiasing_samples);
    const int sampleCount = subPixelOffsets.size();

    // Rasterize with sub-pixel sampling
    for (int y = minY; y <= maxY; y++) {
        const int bufferRowOffset = y * screenWidth;
        const float pixelCenterY = y + 0.5f;
        
        for (int x = minX; x <= maxX; x++) {
            float triangleTotalGrayScale = 0.0f;
            int triangleValidSamples = 0;
            float totalDepth = 0.0f;
            
            const float pixelCenterX = x + 0.5f;
            
            // Sample at each sub-pixel location
            for (int i = 0; i < sampleCount; i++) {
                const float sampleX = pixelCenterX + subPixelOffsets[i].first;
                const float sampleY = pixelCenterY + subPixelOffsets[i].second;
                
                // Edge function method (optimized - no glm::vec2 construction)
                const float w1 = (sampleX - ax) * ab_dy - (sampleY - ay) * ab_dx;
                const float w2 = (sampleX - bx) * bc_dy - (sampleY - by) * bc_dx;
                const float w3 = (sampleX - cx) * ca_dy - (sampleY - cy) * ca_dx;
                
                const bool inside = (w1 >= 0 && w2 >= 0 && w3 >= 0) || (w1 <= 0 && w2 <= 0 && w3 <= 0);
                
                if (inside) {
                    // Calculate barycentric coordinates using pre-computed values
                    const float area1 = abs((bx - sampleX) * (cy - sampleY) - (cx - sampleX) * (by - sampleY));
                    const float area2 = abs((cx - sampleX) * (ay - sampleY) - (ax - sampleX) * (cy - sampleY));
                    
                    const float bw1 = area1 * invTotalArea;
                    const float bw2 = area2 * invTotalArea;
                    const float bw3 = 1.0f - bw1 - bw2; // Third weight (saves computation)
                    
                    // Interpolate depth and texture coordinates
                    const float tex_w = bw1 * vert1_uvw + bw2 * vert2_uvw + bw3 * vert3_uvw;
                    const float invTexW = 1.0f / tex_w;
                    const float tex_uw = (bw1 * vert1_u + bw2 * vert2_u + bw3 * vert3_u) * invTexW;
                    const float tex_vw = (bw1 * vert1_v + bw2 * vert2_v + bw3 * vert3_v) * invTexW;
                    
                    if (tex_uw < 1.0f && tex_vw < 1.0f && tex_uw >= 0.0f && tex_vw >= 0.0f) {
                        // Use bilinear filtered sampling for smoother edges
                        const float sampleGrayScale = tex->GetPixelColFiltered(tex_uw, tex_vw);
                        
                        triangleTotalGrayScale += sampleGrayScale;
                        totalDepth += tex_w;
                        triangleValidSamples++;
                    }
                }
            }
            
            // Calculate final pixel color
            if (triangleValidSamples > 0) {
                const float averageTriangleGrayScale = triangleTotalGrayScale / triangleValidSamples;
                const float averageDepth = totalDepth / triangleValidSamples;
                
                const int bufferIndex = bufferRowOffset + x;
                if (averageDepth > depthBuffer[bufferIndex]) {
                    const CHAR_INFO finalGlyph = GetColGlyph(averageTriangleGrayScale);
                    Screen::GetInstance().PlotPixel(glm::vec2(x, y), finalGlyph);
                    depthBuffer[bufferIndex] = averageDepth;
                }
            }
            // Note: We don't render pure background pixels here since this is partial antialiasing
            // The background should already be cleared before rendering triangles
        }
    }
}

void Renderer::DrawTriangleTexturedAntialiased(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
    int texWidth = tex->GetWidth();
    int texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0) {
        return;
    }

    // Get triangle bounding box
    int minX = std::max(0, (int)std::min({vert1.X(), vert2.X(), vert3.X()}));
    int maxX = std::min((int)Screen::GetInstance().GetWidth() - 1, (int)std::max({vert1.X(), vert2.X(), vert3.X()}));
    int minY = std::max(0, (int)std::min({vert1.Y(), vert2.Y(), vert3.Y()}));
    int maxY = std::min((int)Screen::GetInstance().GetHeight() - 1, (int)std::max({vert1.Y(), vert2.Y(), vert3.Y()}));

    // Pre-compute triangle vertex positions and edge deltas (moved outside loops)
    const float ax = vert1.X(), ay = vert1.Y();
    const float bx = vert2.X(), by = vert2.Y();
    const float cx = vert3.X(), cy = vert3.Y();
    
    // Edge vectors for edge functions
    const float ab_dx = bx - ax, ab_dy = by - ay;
    const float bc_dx = cx - bx, bc_dy = cy - by;
    const float ca_dx = ax - cx, ca_dy = ay - cy;
    
    // Pre-compute triangle area and inverse (for barycentric coordinates)
    const float totalArea = abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay));
    if (totalArea <= 1e-7f) return; // Degenerate triangle
    const float invTotalArea = 1.0f / totalArea;
    
    // Pre-compute vertex attributes
    const float vert1_uvw = vert1.UVW(), vert2_uvw = vert2.UVW(), vert3_uvw = vert3.UVW();
    const float vert1_u = vert1.U(), vert2_u = vert2.U(), vert3_u = vert3.U();
    const float vert1_v = vert1.V(), vert2_v = vert2.V(), vert3_v = vert3.V();
    
    // Cache screen dimensions and depth buffer reference
    const int screenWidth = Screen::GetWidth();
    float* depthBuffer = Screen::GetDepthBuffer();

    // Get dynamic sub-pixel sampling pattern for anti-aliasing
    const auto subPixelOffsets = GenerateSubPixelOffsets(_antialiasing_samples);
    const int sampleCount = subPixelOffsets.size();

    // Rasterize with sub-pixel sampling
    for (int y = minY; y <= maxY; y++) {
        const int bufferRowOffset = y * screenWidth;
        const float pixelCenterY = y + 0.5f;
        
        for (int x = minX; x <= maxX; x++) {
            float triangleTotalGrayScale = 0.0f;
            int triangleValidSamples = 0;
            float totalDepth = 0.0f;
            
            const float pixelCenterX = x + 0.5f;
            
            // Sample at each sub-pixel location
            for (int i = 0; i < sampleCount; i++) {
                const float sampleX = pixelCenterX + subPixelOffsets[i].first;
                const float sampleY = pixelCenterY + subPixelOffsets[i].second;
                
                // Edge function method (optimized - no glm::vec2 construction)
                const float w1 = (sampleX - ax) * ab_dy - (sampleY - ay) * ab_dx;
                const float w2 = (sampleX - bx) * bc_dy - (sampleY - by) * bc_dx;
                const float w3 = (sampleX - cx) * ca_dy - (sampleY - cy) * ca_dx;
                
                const bool inside = (w1 >= 0 && w2 >= 0 && w3 >= 0) || (w1 <= 0 && w2 <= 0 && w3 <= 0);
                
                if (inside) {
                    // Calculate barycentric coordinates using pre-computed values
                    const float area1 = abs((bx - sampleX) * (cy - sampleY) - (cx - sampleX) * (by - sampleY));
                    const float area2 = abs((cx - sampleX) * (ay - sampleY) - (ax - sampleX) * (cy - sampleY));
                    
                    const float bw1 = area1 * invTotalArea;
                    const float bw2 = area2 * invTotalArea;
                    const float bw3 = 1.0f - bw1 - bw2; // Third weight (saves computation)
                    
                    // Interpolate depth and texture coordinates
                    const float tex_w = bw1 * vert1_uvw + bw2 * vert2_uvw + bw3 * vert3_uvw;
                    const float invTexW = 1.0f / tex_w;
                    const float tex_uw = (bw1 * vert1_u + bw2 * vert2_u + bw3 * vert3_u) * invTexW;
                    const float tex_vw = (bw1 * vert1_v + bw2 * vert2_v + bw3 * vert3_v) * invTexW;
                    
                    if (tex_uw < 1.0f && tex_vw < 1.0f && tex_uw >= 0.0f && tex_vw >= 0.0f) {
                        // Use bilinear filtered sampling for smoother edges
                        const float sampleGrayScale = tex->GetPixelColFiltered(tex_uw, tex_vw);
                        
                        triangleTotalGrayScale += sampleGrayScale;
                        totalDepth += tex_w;
                        triangleValidSamples++;
                    }
                }
            }
            
            // Calculate final pixel color
            if (triangleValidSamples > 0) {
                const float averageTriangleGrayScale = triangleTotalGrayScale / triangleValidSamples;
                const float averageDepth = totalDepth / triangleValidSamples;
                
                const int bufferIndex = bufferRowOffset + x;
                if (averageDepth > depthBuffer[bufferIndex]) {
                    const CHAR_INFO finalGlyph = GetColGlyph(averageTriangleGrayScale);
                    Screen::GetInstance().PlotPixel(glm::vec2(x, y), finalGlyph);
                    depthBuffer[bufferIndex] = averageDepth;
                }
            }
        }
    }
}

void Renderer::DrawLine(const int x1, const int y1, const int x2, const int y2, const CHAR pixel_type, const short col) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int incx = (x2 > x1) ? 1 : -1;
    int incy = (y2 > y1) ? 1 : -1;
    int x = x1, y = y1;

    Screen::GetInstance().PlotPixel(x, y, pixel_type, col);

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
            Screen::GetInstance().PlotPixel(x, y, pixel_type, col);
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
            Screen::GetInstance().PlotPixel(x, y, pixel_type, col);
        }
    }
}

void Renderer::DrawTriangleWireFrame(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, CHAR pixel_type, short col) {
	// RENDERING LINES BETWEEN VERTICES
	DrawLine((int) vert1.X(), (int) vert1.Y(), (int) vert2.X(), (int) vert2.Y(), pixel_type, col);
	DrawLine((int) vert2.X(), (int) vert2.Y(), (int) vert3.X(), (int) vert3.Y(), pixel_type, col);
	DrawLine((int) vert3.X(), (int) vert3.Y(), (int) vert1.X(), (int) vert1.Y(), pixel_type, col);
}

// =============================================================================
// COORDINATE TRANSFORMATION FUNCTIONS
// =============================================================================

void Renderer::ViewPortTransform(VERTEX& vertice) {
	// transforms vertice from [-1 to 1] to [0 to scr_dim]
	glm::vec4 newPos = glm::vec4(((vertice.X() + 1.0f) / 2.0f) * Screen::GetInstance().GetWidth(), ((vertice.Y() + 1.0f) / 2.0f) * Screen::GetInstance().GetHeight(), vertice.Z(), vertice.W());
	vertice.SetXYZW(newPos);
}

void Renderer::PerspectiveDivision(VERTEX& clipCoord) {
    // Divide position and UVW by w for perspective correction
    float w = clipCoord.W();
    clipCoord.data[0] /= w; // x
    clipCoord.data[1] /= w; // y
    clipCoord.data[2] /= w; // z
    clipCoord.data[3] = w;  // w stays the same

    glm::vec3 uvw = clipCoord.GetUVW();
    clipCoord.SetUVW(glm::vec3(uvw.x / w, uvw.y / w, 1.0f / w));
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

void Renderer::ClippingHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& clipped) {
    static const int components[6] = {2, 2, 1, 1, 0, 0};
    static const bool nears[6]     = {true, false, true, false, true, false};

    std::vector<VERTEX> tempA = vertices;
    std::vector<VERTEX> tempB;

    for (int i = 0; i < 6; ++i) {
        tempB = Clipping(tempA, components[i], nears[i]);
        tempA = std::move(tempB);
    }
    clipped = std::move(tempA);
}

std::vector<VERTEX> Renderer::Clipping(const std::vector<VERTEX>& vertices, const int component, const bool Near) {
    std::vector<VERTEX> clipped;

    for (int i = 0; i < static_cast<int>(vertices.size()); i += 3)
    {
        std::vector<int> inside;
        std::vector<int> outside;

        VERTEX temp[6];
        int newTri = 0;

        // Use direct array indexing for w and component
        auto isInside = [&](const VERTEX& v) {
            float w = v.W();
            float val = v.data[component];
            return Near ? (val > -w) : (val < w);
        };

        if (isInside(vertices[i]))      inside.push_back(i);      else outside.push_back(i);
        if (isInside(vertices[i + 1]))  inside.push_back(i + 1);  else outside.push_back(i + 1);
        if (isInside(vertices[i + 2]))  inside.push_back(i + 2);  else outside.push_back(i + 2);

        if (inside.size() == 3) {
            clipped.push_back(vertices[inside[0]]);
            clipped.push_back(vertices[inside[1]]);
            clipped.push_back(vertices[inside[2]]);
            continue;
        }
        else if (inside.size() == 1) {
            VERTEX newPos1 = HomogenousPlaneIntersect(vertices[inside[0]], vertices[outside[0]], component, Near);
            VERTEX newPos2 = HomogenousPlaneIntersect(vertices[inside[0]], vertices[outside[1]], component, Near);

            temp[outside[0] - i] = newPos1;
            temp[outside[1] - i] = newPos2;
            temp[inside[0] - i] = vertices[inside[0]];
            newTri = 3;
        }
        else if (inside.size() == 2) {
            VERTEX newPos1 = HomogenousPlaneIntersect(vertices[inside[0]], vertices[outside[0]], component, Near);
            VERTEX newPos2 = HomogenousPlaneIntersect(vertices[inside[1]], vertices[outside[0]], component, Near);

            // triangle 1
            temp[inside[0] - i] = vertices[inside[0]];
            temp[inside[1] - i] = vertices[inside[1]];
            temp[outside[0] - i] = newPos1;

            // triangle 2
            temp[outside[0] - i + 3] = newPos2;
            temp[inside[0] - i + 3] = newPos1;
            temp[inside[1] - i + 3] = vertices[inside[1]];
            newTri = 6;
        }
        
        for (int k = 0; k < newTri; k++) {
            clipped.push_back(temp[k]);
        }
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

    for (int i = 0; i < 6; i++)
        newVert.data[i] = glm::mix(vert1.data[i], vert2.data[i], t); // interpolate attributes

    return newVert;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

glm::mat4 Renderer::CalcModelMatrix(const glm::vec3 position, const glm::vec2 rotation, const glm::vec3 size) {
	// this function just saves lines as this needs to be calculated alot
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

CHAR_INFO Renderer::GetColGlyph(const float GreyScale) {
    static const unsigned int numShades = 16;
    static const CHAR_INFO vals[numShades] = {
        CHAR_INFO{ PIXEL_QUARTER, FG_BLACK}, CHAR_INFO{ PIXEL_QUARTER, FG_DARK_GREY},
        CHAR_INFO{ PIXEL_QUARTER, FG_GREY}, CHAR_INFO{ PIXEL_QUARTER, FG_WHITE},
        CHAR_INFO{ PIXEL_HALF, FG_BLACK}, CHAR_INFO{ PIXEL_HALF, FG_DARK_GREY},
        CHAR_INFO{ PIXEL_HALF, FG_GREY}, CHAR_INFO{ PIXEL_HALF, FG_WHITE},
        CHAR_INFO{ PIXEL_THREEQUARTERS, FG_BLACK}, CHAR_INFO{ PIXEL_THREEQUARTERS, FG_DARK_GREY},
        CHAR_INFO{ PIXEL_THREEQUARTERS, FG_GREY}, CHAR_INFO{ PIXEL_THREEQUARTERS, FG_WHITE},
        CHAR_INFO{ PIXEL_FULL, FG_BLACK}, CHAR_INFO{ PIXEL_FULL, FG_DARK_GREY},
        CHAR_INFO{ PIXEL_FULL, FG_GREY}, CHAR_INFO{ PIXEL_FULL, FG_WHITE},
    };

    int idx = static_cast<int>(GreyScale * numShades);
    if (idx > numShades - 1) idx = numShades - 1;
    if (idx < 0) idx = 0;
    return vals[idx];
}

float Renderer::GrayScaleRGB(const glm::vec3 rgb)
{
	return (0.3f * rgb.x + 0.6f * rgb.y + 0.1f * rgb.z); // grayscales based on how much we see that wavelength of light instead of just averaging
}

void Renderer::SetAntialiasingsamples(int samples) {
    // Clamp to reasonable values (1 to 16 samples)
    _antialiasing_samples = std::max(1, std::min(16, samples));
}

int Renderer::GetAntialiasingsamples() {
    return _antialiasing_samples;
}

void Renderer::SetWireframe(bool wireframe) {
    _wireframe = wireframe;
}

bool Renderer::GetWireframe() {
    return _wireframe;
}

void Renderer::SetBackfaceCulling(bool backfaceCulling) {
    _backface_culling = backfaceCulling;
}

bool Renderer::GetBackfaceCulling() {
    return _backface_culling;
}

void Renderer::SetCCW(bool ccw) {
    _ccw = ccw;
}

bool Renderer::GetCCW() {
    return _ccw;
}

void Renderer::SetAntialiasing(bool antialiasing) {
    _antialiasing = antialiasing;
}

bool Renderer::GetAntialiasing() {
    return _antialiasing;
}

void Renderer::InvalidateTiles() {
    _tilesInitialized = false;
}

std::vector<std::pair<float, float>> Renderer::GenerateSubPixelOffsets(int sampleCount) {
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
    
    return offsets;
}