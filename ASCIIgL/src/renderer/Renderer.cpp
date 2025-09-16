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

void Renderer::DrawMesh(VERTEX_SHADER& VSHADER, const Mesh* mesh, const glm::vec3 position, const glm::vec3 rotation, const glm::vec3 size, const Camera3D& camera) {
    glm::mat4 model = CalcModelMatrix(position, rotation, size);

    VSHADER.SetMatrices(model, camera.view, camera.proj);

    if (mesh->texture) {
        RenderTriangles(VSHADER, mesh->vertices, mesh->texture);
    }
}



void Renderer::DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::vec3 position, const glm::vec3 rotation, const glm::vec3 size, const Camera3D& camera) {
    glm::mat4 model = CalcModelMatrix(position, rotation, size);

    VSHADER.SetMatrices(model, camera.view, camera.proj);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(VSHADER, ModelObj.meshes[i]);
    }
}

void Renderer::DrawModel(VERTEX_SHADER& VSHADER, const Model& ModelObj, const glm::mat4 model, const Camera3D& camera)
{
    VSHADER.SetMatrices(model, camera.view, camera.proj);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(VSHADER, ModelObj.meshes[i]);
    }
}

void Renderer::Draw2DQuadPixelSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2 position, const float rotation, const glm::vec2 size, const Camera2D& camera, int layer)
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

void Renderer::Draw2DQuadPercSpace(VERTEX_SHADER& VSHADER, const Texture& tex, const glm::vec2 positionPerc, const float rotation, const glm::vec2 sizePerc, const Camera2D& camera, int layer)
{
    // Convert percentage coordinates to pixel coordinates
    float screenWidth = static_cast<float>(Screen::GetInstance().GetVisibleWidth());
    float screenHeight = static_cast<float>(Screen::GetInstance().GetHeight());
    
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

void Renderer::DrawScreenBorder(const short col) {
	// DRAWING BORDERS
	DrawLine(1, 1, Screen::GetInstance().GetVisibleWidth() - 1, 1, static_cast<CHAR>(PX_TYPE::PX_FULL), col);
	DrawLine(Screen::GetInstance().GetVisibleWidth() - 1, 1, Screen::GetInstance().GetVisibleWidth() - 1, Screen::GetInstance().GetHeight() - 1, static_cast<CHAR>(PX_TYPE::PX_FULL), col);
	DrawLine(Screen::GetInstance().GetVisibleWidth() - 1, Screen::GetInstance().GetHeight() - 1, 1, Screen::GetInstance().GetHeight() - 1, static_cast<CHAR>(PX_TYPE::PX_FULL), col);
	DrawLine(1, 1, 1, Screen::GetInstance().GetHeight() - 1, static_cast<CHAR>(PX_TYPE::PX_FULL), col);
}

// =============================================================================
// CORE RENDERING PIPELINE
// =============================================================================

void Renderer::RenderTriangles(const VERTEX_SHADER& VSHADER, const std::vector<VERTEX>& vertices, const Texture* tex) {
    if (vertices.size() < 3 || vertices.size() % 3 != 0) {
        return;
    }

    // Validate texture integrity if provided
    if (tex != nullptr) {
        try {
            int width = tex->GetWidth();
            int height = tex->GetHeight();
            
            if (width <= 0 || height <= 0) {
                Logger::Error("RenderTriangles: Invalid texture dimensions - Width: " + std::to_string(width) + ", Height: " + std::to_string(height));
                return;
            }
            
            if (width > 4096 || height > 4096) {
                Logger::Error("RenderTriangles: Texture dimensions too large - Width: " + std::to_string(width) + ", Height: " + std::to_string(height));
                return;
            }
            
            // Test if we can safely access texture data by sampling a corner pixel
            glm::vec3 testPixel = tex->GetPixelRGB(0.0f, 0.0f);
            
        } catch (const std::exception& e) {
            Logger::Error("RenderTriangles: Exception during texture validation - " + std::string(e.what()));
            return;
        } catch (...) {
            Logger::Error("RenderTriangles: Unknown exception during texture validation");
            return;
        }
    }

    // Optimize: Use pre-allocated buffers instead of creating new vectors each frame
    _vertexBuffer.clear();
    _vertexBuffer.reserve(vertices.size());
    _vertexBuffer = vertices;

    const_cast<VERTEX_SHADER&>(VSHADER).GLUseBatch(_vertexBuffer);

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

    if (!_tilesInitialized) {
        InitializeTiles();
        _tilesInitialized = true;
    } else {
        ClearTileTriangleLists();
    }

    BinTrianglesToTiles(_rasterBuffer);

    std::for_each(std::execution::par, _tileBuffer.begin(), _tileBuffer.end(), [&](Tile& tile) {
        if (_wireframe || tex == nullptr) {
            DrawTileWireframe(tile, _rasterBuffer);
        } else {
            DrawTileTextured(tile, _rasterBuffer, tex);
        }
    });
}

void Renderer::BackFaceCullHelper(const std::vector<VERTEX>& vertices, std::vector<VERTEX>& raster_triangles) {
    if (vertices.size() < 3) return;

    const size_t triangleCount = vertices.size() / 3;
    
    // Create a parallel char array to mark which triangles to keep (thread-safe)
    std::vector<char> keepTriangle(triangleCount);
    
    // Create index vector for parallel processing
    std::vector<size_t> triangleIndices(triangleCount);
    std::iota(triangleIndices.begin(), triangleIndices.end(), 0);
    
    // Parallel pass: determine which triangles to keep (no data races)
    std::for_each(std::execution::par, 
        triangleIndices.begin(), 
        triangleIndices.end(),
        [&keepTriangle, &vertices, ccw = _ccw](size_t triIndex) {  // Capture by value
            const size_t vertexIndex = triIndex * 3;
            keepTriangle[triIndex] = !BackFaceCull(
                vertices[vertexIndex], 
                vertices[vertexIndex + 1], 
                vertices[vertexIndex + 2], 
                ccw  // Use local copy
            );
        });
    
    // Sequential pass: collect kept triangles (avoids race conditions on push_back)
    // Reserve space to minimize allocations
    size_t keepCount = std::count(keepTriangle.begin(), keepTriangle.end(), 1);
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

void Renderer::InitializeTiles() {
    // One-time initialization of tile structure - much faster than recreating every frame
    int tileCount = Screen::GetInstance().GetTileCountX() * Screen::GetInstance().GetTileCountY();
    _tileBuffer.clear();
    _tileBuffer.resize(tileCount);
    
    for (int ty = 0; ty < Screen::GetInstance().GetTileCountY(); ++ty) {
        for (int tx = 0; tx < Screen::GetInstance().GetTileCountX(); ++tx) {
            int tileIndex = ty * Screen::GetInstance().GetTileCountX() + tx;
            int posX = tx * Screen::GetInstance().GetTileSizeX();
            int posY = ty * Screen::GetInstance().GetTileSizeY();

            // Clamp tile size if it would overflow the screen
            int sizeX = std::min(Screen::GetInstance().GetTileSizeX(), Screen::GetInstance().GetVisibleWidth() - posX);
            int sizeY = std::min(Screen::GetInstance().GetTileSizeY(), Screen::GetInstance().GetHeight() - posY);

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

void Renderer::BinTrianglesToTiles(const std::vector<VERTEX>& raster_triangles) {
    // Optimized version that works directly with pre-allocated tile buffer
    auto& screen = Screen::GetInstance();
    const int tileSizeX = screen.GetTileSizeX();
    const int tileSizeY = screen.GetTileSizeY();
    const int tileCountX = screen.GetTileCountX();
    const int tileCountY = screen.GetTileCountY();
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
        int maxTileX = std::min(tileCountX - 1, static_cast<int>(std::ceil(maxTriPt.x * invTileSizeX)));
        int minTileY = std::max(0, static_cast<int>(minTriPt.y * invTileSizeY));
        int maxTileY = std::min(tileCountY - 1, static_cast<int>(std::ceil(maxTriPt.y * invTileSizeY)));

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

void Renderer::DrawTileWireframe(const Tile& tile, const std::vector<VERTEX>& raster_triangles) {
    // Draw wireframe for each triangle in the tile
    for (int triIndex : tile.tri_indices_encapsulated) {
        DrawTriangleWireframe(raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], static_cast<CHAR>(PX_TYPE::PX_FULL), FG_WHITE);
    }
    for (int triIndex : tile.tri_indices_partial) {
        DrawTriangleWireframePartial(tile, raster_triangles[triIndex], raster_triangles[triIndex + 1], raster_triangles[triIndex + 2], static_cast<CHAR>(PX_TYPE::PX_FULL), FG_WHITE);
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
    auto& screen = Screen::GetInstance();
    const int screenWidth = screen.GetVisibleWidth();
    const int screenHeight = screen.GetHeight();
    float* const depthBuffer = screen.GetDepthBuffer();

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
                        // Use reciprocal multiplication instead of division (much faster)
                        float inv_tex_w = 1.0f / tex_w;
                        float tex_uw = ((1.0f - t) * tex_su + t * tex_eu) * inv_tex_w;
                        float tex_vw = ((1.0f - t) * tex_sv + t * tex_ev) * inv_tex_w;
                        
                        if (tex_uw < 1.0f && tex_vw < 1.0f && tex_uw >= 0.0f && tex_vw >= 0.0f) {
                            // Use integer texture coordinates for better cache performance
                            float texWidthProd = tex_uw * texWidth;
                            float texHeightProd = tex_vw * texHeight;

                            if (_grayscale) {
                                float grayscaleCol = tex->GetPixelGrayscale(texWidthProd, texHeightProd);
                                screen.PlotPixel(glm::vec2(j, i), GetColGlyphGreyScale(grayscaleCol));
                            } else {
                                glm::vec3 rgbCol = tex->GetPixelRGB(texWidthProd, texHeightProd);
                                screen.PlotPixel(glm::vec2(j, i), GetColGlyph(rgbCol));
                            }
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
    auto& screen = Screen::GetInstance();
    const int screenWidth = screen.GetVisibleWidth();
    const int screenHeight = screen.GetHeight();
    float* const depthBuffer = screen.GetDepthBuffer();

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
                        // Use reciprocal multiplication instead of division (much faster)
                        float inv_tex_w = 1.0f / tex_w;
                        float tex_uw = ((1.0f - t) * tex_su + t * tex_eu) * inv_tex_w;
                        float tex_vw = ((1.0f - t) * tex_sv + t * tex_ev) * inv_tex_w;
                        
                        if (tex_uw < 1.0f && tex_vw < 1.0f && tex_uw >= 0.0f && tex_vw >= 0.0f) {
                            float texWidthProd = tex_uw * texWidth;
                            float texHeightProd = tex_vw * texHeight;
                            if (_grayscale) {
                                float grayscaleCol = tex->GetPixelGrayscale(texWidthProd, texHeightProd);
                                screen.PlotPixel(glm::vec2(j, i), GetColGlyphGreyScale(grayscaleCol));
                            } else {
                                glm::vec3 rgbCol = tex->GetPixelRGB(texWidthProd, texHeightProd);
                                screen.PlotPixel(glm::vec2(j, i), GetColGlyph(rgbCol));
                            }
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

void Renderer::DrawTriangleWireframePartial(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, CHAR pixel_type, short col) {
    // Get tile bounds for clipping
    const int minX = static_cast<int>(tile.position.x);
    const int maxX = static_cast<int>(tile.position.x + tile.size.x);
    const int minY = static_cast<int>(tile.position.y);
    const int maxY = static_cast<int>(tile.position.y + tile.size.y);
    
    // Helper function to draw a line segment clipped to the tile bounds
    auto drawClippedLine = [&](int x0, int y0, int x1, int y1) {
        // Simple line clipping - only draw pixels within tile bounds
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int incx = (x1 > x0) ? 1 : -1;
        int incy = (y1 > y0) ? 1 : -1;
        int x = x0, y = y0;

        // Plot first pixel if within bounds
        if (x >= minX && x < maxX && y >= minY && y < maxY) {
            Screen::GetInstance().PlotPixel(x, y, pixel_type, col);
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
                    Screen::GetInstance().PlotPixel(x, y, pixel_type, col);
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
                    Screen::GetInstance().PlotPixel(x, y, pixel_type, col);
                }
            }
        }
    };
    
    // Draw the three edges of the triangle with tile clipping
    drawClippedLine(static_cast<int>(vert1.X()), static_cast<int>(vert1.Y()), 
                    static_cast<int>(vert2.X()), static_cast<int>(vert2.Y()));
    drawClippedLine(static_cast<int>(vert2.X()), static_cast<int>(vert2.Y()), 
                    static_cast<int>(vert3.X()), static_cast<int>(vert3.Y()));
    drawClippedLine(static_cast<int>(vert3.X()), static_cast<int>(vert3.Y()), 
                    static_cast<int>(vert1.X()), static_cast<int>(vert1.Y()));
}

void Renderer::DrawTriangleTexturedPartialAntialiased(const Tile& tile, const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, const Texture* tex) {
    if (!tex) { Logger::Error("  Texture is nullptr!"); return; }
    int texWidth, texHeight;
    try { texWidth = tex->GetWidth(); texHeight = tex->GetHeight(); }
    catch (...) { Logger::Error("  Exception caught when accessing texture methods!"); return; }
    if (texWidth == 0 || texHeight == 0) { Logger::Error("  Invalid texture dimensions!"); return; }

    // Get triangle bounding box clipped to tile
    int minX = std::max((int)tile.position.x, (int)std::min({vert1.X(), vert2.X(), vert3.X()}));
    int maxX = std::min((int)(tile.position.x + tile.size.x) - 1, (int)std::max({vert1.X(), vert2.X(), vert3.X()}));
    int minY = std::max((int)tile.position.y, (int)std::min({vert1.Y(), vert2.Y(), vert3.Y()}));
    int maxY = std::min((int)(tile.position.y + tile.size.y) - 1, (int)std::max({vert1.Y(), vert2.Y(), vert3.Y()}));
    minX = std::max(0, minX);
    maxX = std::min((int)Screen::GetInstance().GetVisibleWidth() - 1, maxX);
    minY = std::max(0, minY);
    maxY = std::min((int)Screen::GetInstance().GetHeight() - 1, maxY);
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

    // Sub-pixel pattern
    const auto subPixelOffsets = GetSubpixelOffsets();
    const int sampleCount = (int)subPixelOffsets.size();

    Screen& screen = Screen::GetInstance();
    float* depth = screen.GetDepthBuffer();
    const int screenWidth = screen.GetVisibleWidth();

    for (int y = minY; y <= maxY; ++y) {
        const float baseY = y + 0.5f;
        const int rowOffset = y * screenWidth;
        for (int x = minX; x <= maxX; ++x) {
            const float baseX = x + 0.5f;
            glm::vec3 rgbAccum(0.0f); float grayAccum = 0.0f; float depthAccum = 0.0f; int insideSamples = 0;

            // Pixel center edge evaluations
            const float e12c = A12 * baseX + B12 * baseY + C12;
            const float e23c = A23 * baseX + B23 * baseY + C23;
            const float e31c = A31 * baseX + B31 * baseY + C31;

            // If clearly outside (all negative beyond margin) skip
            if (e12c < -1.1f && e23c < -1.1f && e31c < -1.1f) continue;

            for (int si = 0; si < sampleCount; ++si) {
                const float sx = baseX + subPixelOffsets[si].first;
                const float sy = baseY + subPixelOffsets[si].second;
                const float e12 = A12 * sx + B12 * sy + C12;
                const float e23 = A23 * sx + B23 * sy + C23;
                const float e31 = A31 * sx + B31 * sy + C31;
                if (e12 < 0.f || e23 < 0.f || e31 < 0.f) continue;
                const float wA = e23 * invSignedArea;
                const float wB = e31 * invSignedArea;
                const float wC = 1.0f - wA - wB;
                const float perspW = wA*w1p + wB*w2p + wC*w3p;
                const float invPerspW = 1.0f / perspW;
                const float tu = (wA*u1 + wB*u2 + wC*u3) * invPerspW;
                const float tv = (wA*v1t + wB*v2t + wC*v3t) * invPerspW;
                if (tu < 0.f || tu >= 1.f || tv < 0.f || tv >= 1.f) continue;
                const float sX = tu * texWidth;
                const float sY = tv * texHeight;
                if (_grayscale) {
                    grayAccum += tex->GetPixelGrayscale(sX, sY);
                } else {
                    rgbAccum += tex->GetPixelRGB(sX, sY);
                }
                depthAccum += perspW;
                ++insideSamples;
            }

            if (insideSamples) {
                const int idx = rowOffset + x;
                const float avgDepth = depthAccum / insideSamples;
                if (avgDepth > depth[idx]) {
                    if (_grayscale) {
                        const float g = grayAccum / insideSamples;
                        screen.PlotPixel(glm::vec2(x, y), GetColGlyphGreyScale(g));
                    } else {
                        const glm::vec3 c = rgbAccum * (1.0f / insideSamples);
                        screen.PlotPixel(glm::vec2(x, y), GetColGlyph(c));
                    }
                    depth[idx] = avgDepth;
                }
            }
        }
    }
}

void Renderer::DrawTriangleTexturedAntialiased(const VERTEX& v1, const VERTEX& v2, const VERTEX& v3, const Texture* tex) {
    if (!tex) { Logger::Error("  Texture is nullptr!"); return; }

    const int texWidth = tex->GetWidth();
    const int texHeight = tex->GetHeight();
    if (texWidth == 0 || texHeight == 0) return;

    // Triangle bounding box (clamped)
    int minX = std::max(0, (int)std::floor(std::min({ v1.X(), v2.X(), v3.X() })));
    int maxX = std::min((int)Screen::GetInstance().GetVisibleWidth() - 1, (int)std::ceil (std::max({ v1.X(), v2.X(), v3.X() })));
    int minY = std::max(0, (int)std::floor(std::min({ v1.Y(), v2.Y(), v3.Y() })));
    int maxY = std::min((int)Screen::GetInstance().GetHeight()       - 1, (int)std::ceil (std::max({ v1.Y(), v2.Y(), v3.Y() })));
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

    // Sub-pixel pattern
    const auto subPixelOffsets = GetSubpixelOffsets();
    const int sampleCount = (int)subPixelOffsets.size();

    Screen& screen = Screen::GetInstance();
    float* depth = screen.GetDepthBuffer();
    const int screenWidth = screen.GetVisibleWidth();

    for (int y = minY; y <= maxY; ++y) {
        const float baseY = y + 0.5f;
        const int rowOffset = y * screenWidth;
        for (int x = minX; x <= maxX; ++x) {
            const float baseX = x + 0.5f;

            glm::vec3 rgbAccum(0.0f); float grayAccum = 0.0f; float depthAccum = 0.0f; int insideSamples = 0;

            // Pixel center edge evaluations
            const float e12c = A12 * baseX + B12 * baseY + C12;
            const float e23c = A23 * baseX + B23 * baseY + C23;
            const float e31c = A31 * baseX + B31 * baseY + C31;

            // If clearly outside (all negative beyond margin) skip
            if (e12c < -1.1f && e23c < -1.1f && e31c < -1.1f) continue;

            // Multisample path
            for (int si = 0; si < sampleCount; ++si) {
                const float sx = baseX + subPixelOffsets[si].first;
                const float sy = baseY + subPixelOffsets[si].second;
                const float e12 = A12 * sx + B12 * sy + C12;
                const float e23 = A23 * sx + B23 * sy + C23;
                const float e31 = A31 * sx + B31 * sy + C31;
                if (e12 < 0.f || e23 < 0.f || e31 < 0.f) continue;
                const float wA = e23 * invSignedArea;
                const float wB = e31 * invSignedArea;
                const float wC = 1.0f - wA - wB;
                const float perspW = wA*w1p + wB*w2p + wC*w3p;
                const float invPerspW = 1.0f / perspW;
                const float tu = (wA*u1 + wB*u2 + wC*u3) * invPerspW;
                const float tv = (wA*v1t + wB*v2t + wC*v3t) * invPerspW;
                if (tu < 0.f || tu >= 1.f || tv < 0.f || tv >= 1.f) continue;
                const float sX = tu * texWidth;
                const float sY = tv * texHeight;
                if (_grayscale) {
                    grayAccum += tex->GetPixelGrayscale(sX, sY);
                } else {
                    rgbAccum += tex->GetPixelRGB(sX, sY);
                }
                depthAccum += perspW;
                ++insideSamples;
            }

            if (insideSamples) {
                const int idx = rowOffset + x;
                const float avgDepth = depthAccum / insideSamples;
                if (avgDepth > depth[idx]) {
                    if (_grayscale) {
                        const float g = grayAccum / insideSamples;
                        screen.PlotPixel(glm::vec2(x, y), GetColGlyphGreyScale(g));
                    } else {
                        const glm::vec3 c = rgbAccum * (1.0f / insideSamples);
                        screen.PlotPixel(glm::vec2(x, y), GetColGlyph(c));
                    }
                    depth[idx] = avgDepth;
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

void Renderer::DrawTriangleWireframe(const VERTEX& vert1, const VERTEX& vert2, const VERTEX& vert3, CHAR pixel_type, short col) {
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
	// Flip Y to match screen coordinates where Y=0 is at top
	glm::vec4 newPos = glm::vec4(
		((vertice.X() + 1.0f) / 2.0f) * Screen::GetInstance().GetVisibleWidth(), 
		((1.0f - vertice.Y()) / 2.0f) * Screen::GetInstance().GetHeight(),  // Flipped Y
		vertice.Z(), 
		vertice.W()
	);
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

glm::mat4 Renderer::CalcModelMatrix(const glm::vec3 position, const glm::vec3 rotation, const glm::vec3 size) {
	// this function just saves lines as this needs to be calculated alot
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch (X-axis)
	model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw (Y-axis)
	model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z-axis)
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

glm::mat4 Renderer::CalcModelMatrix(const glm::vec3 position, const float rotation, const glm::vec3 size) {
	// this function just saves lines as this needs to be calculated alot
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, -0.5f * size.z));
	model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z-axis)
	model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.5f * size.z));
	model = glm::scale(model, size);

	return model;
}

CHAR_INFO Renderer::GetColGlyphGreyScale(const float greyscale) {
    static const unsigned int numShades = 16;
    static const CHAR_INFO greyScaleGlyphs[numShades] = {
        CHAR_INFO{ '\0', BG_BLACK}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_QUARTER), FG_DARK_GREY},
        CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_QUARTER), FG_GREY}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_QUARTER), FG_WHITE},
        CHAR_INFO{ '\0', BG_BLACK}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_HALF), FG_DARK_GREY},
        CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_HALF), FG_GREY}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_HALF), FG_WHITE},
        CHAR_INFO{ '\0', BG_BLACK}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_THREEQUARTERS), FG_DARK_GREY},
        CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_THREEQUARTERS), FG_GREY}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_THREEQUARTERS), FG_WHITE},
        CHAR_INFO{ '\0', BG_BLACK}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_FULL), FG_DARK_GREY},
        CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_FULL), FG_GREY}, CHAR_INFO{ static_cast<wchar_t>(PX_TYPE::PX_FULL), FG_WHITE},
    };

    int idx = static_cast<int>(greyscale * numShades);
    if (idx > numShades - 1) idx = numShades - 1;
    if (idx < 0) idx = 0;
    return greyScaleGlyphs[idx];
}

CHAR_INFO Renderer::GetColGlyph(const glm::vec3 rgb) {
    // Pre-calculate intensity once using optimized luminance weights
    const float intensity = GrayScaleRGB(rgb);
    
    // Early exit for very dark colors - use black background
    if (intensity < 0.1f) {
        return CHAR_INFO{' ', BG_BLACK};
    }
    
    // Find closest color using unrolled loop and manual comparison for better cache performance
    float minDistSq = FLT_MAX;
    int bestColorIndex = 0;
    
    // Unroll the loop partially to reduce loop overhead and improve branch prediction
    const float r = rgb.r, g = rgb.g, b = rgb.b;
    
    // Check colors 0-7 (dark colors)
    for (int i = 0; i < 8; ++i) {
        const float dr = r - consoleColors[i].x;
        const float dg = g - consoleColors[i].y;
        const float db = b - consoleColors[i].z;
        const float distSq = dr * dr + dg * dg + db * db;
        
        if (distSq < minDistSq) {
            minDistSq = distSq;
            bestColorIndex = i;
        }
    }
    
    // Check colors 8-15 (bright colors)
    for (int i = 8; i < 16; ++i) {
        const float dr = r - consoleColors[i].x;
        const float dg = g - consoleColors[i].y;
        const float db = b - consoleColors[i].z;
        const float distSq = dr * dr + dg * dg + db * db;
        
        if (distSq < minDistSq) {
            minDistSq = distSq;
            bestColorIndex = i;
        }
    }
    
    // Optimized glyph selection using bit manipulation instead of branches
    const int glyphIndex = static_cast<int>(intensity * 4.0f); // Map [0,1] to [0,4)
    static const wchar_t glyphs[4] = {static_cast<wchar_t>(PX_TYPE::PX_QUARTER), static_cast<wchar_t>(PX_TYPE::PX_HALF), static_cast<wchar_t>(PX_TYPE::PX_THREEQUARTERS), static_cast<wchar_t>(PX_TYPE::PX_FULL)};
    const wchar_t glyph = glyphs[glyphIndex > 3 ? 3 : glyphIndex]; // Clamp to valid range
    
    return CHAR_INFO{glyph, colorCodes[bestColorIndex]};
}

CHAR_INFO Renderer::GetColGlyph(const glm::vec3 rgb, const float greyscale) {
    
    // Find closest color using squared distance (avoid sqrt for speed)
    float minDistSq = FLT_MAX;
    int bestColorIndex = 0;
    
    for (int i = 0; i < 16; ++i) {
        const glm::vec3 diff = rgb - consoleColors[i];
        const float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            bestColorIndex = i;
        }
    }
    
    // Select glyph based on pre-calculated greyscale intensity (4 levels)
    wchar_t glyph;
    if (greyscale < 0.25f) {
        glyph = static_cast<wchar_t>(PX_TYPE::PX_QUARTER);
    } else if (greyscale < 0.5f) {
        glyph = static_cast<wchar_t>(PX_TYPE::PX_HALF);
    } else if (greyscale < 0.75f) {
        glyph = static_cast<wchar_t>(PX_TYPE::PX_THREEQUARTERS);
    } else {
        glyph = static_cast<wchar_t>(PX_TYPE::PX_FULL);
    }
    
    // For very dark colors, use background color instead of foreground with glyph
    if (greyscale < 0.1f) {
        return CHAR_INFO{' ', static_cast<unsigned short>(colorCodes[bestColorIndex] << 4)}; // Shift to background
    }
    
    return CHAR_INFO{glyph, colorCodes[bestColorIndex]};
}

float Renderer::GrayScaleRGB(const glm::vec3 rgb)
{
	return (0.299f * rgb.r + 0.587f * rgb.g + 0.114f * rgb.b); // grayscales based on how much we see that wavelength of light instead of just averaging
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

void Renderer::SetGrayscale(bool grayscale) {
    _grayscale = grayscale;
}

bool Renderer::GetGrayscale() {
    return _grayscale;
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

std::vector<std::pair<float, float>> Renderer::GetSubpixelOffsets() {
    if (_subpixel_offsets.empty()) {
        _subpixel_offsets = GenerateSubPixelOffsets(_antialiasing_samples);
    }
    return _subpixel_offsets;
}