#include <ASCIIgL/renderer/TileManager.hpp>
#include <ASCIIgL/renderer/Screen.hpp>
#include <ASCIIgL/util/MathUtil.hpp>

void TileManager::InitializeTiles(unsigned int screen_width, unsigned int screen_height) {
    CalculateTileCounts(screen_width, screen_height);
    int tileCount = GetTileCountX() * GetTileCountY();
    activeTiles.clear();
    activeTiles.reserve(tileCount);
    _tileBuffer.clear();
    _tileBuffer.resize(tileCount);

    for (int ty = 0; ty < GetTileCountY(); ++ty) {
        for (int tx = 0; tx < GetTileCountX(); ++tx) {
            int tileIndex = ty * GetTileCountX() + tx;
            int posX = tx * GetTileSizeX();
            int posY = ty * GetTileSizeY();

            // Clamp tile size if it would overflow the screen
            int sizeX = std::min(GetTileSizeX(), screen_width - posX);
            int sizeY = std::min(GetTileSizeY(), screen_height - posY);

            _tileBuffer[tileIndex].position = glm::ivec2(posX, posY);
            _tileBuffer[tileIndex].size = glm::ivec2(sizeX, sizeY);
            
            // Pre-reserve space to avoid frequent reallocations
            _tileBuffer[tileIndex].tri_indices_encapsulated.reserve(64);
            _tileBuffer[tileIndex].tri_indices_partial.reserve(64);
        }
    }
}

void TileManager::ClearTileTriangleLists() {
    // Much faster than recreating tiles - just clear the triangle lists
    for (auto& tile : _tileBuffer) {
        tile.tri_indices_encapsulated.clear();
        tile.tri_indices_partial.clear();
    }
}

bool TileManager::DoesTileEncapsulate(const Tile& tile, const VERTEX& v1, const VERTEX& v2, const VERTEX& v3)
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

void TileManager::BinTrianglesToTiles(const std::vector<VERTEX>& raster_triangles) {
    for (auto& tile : _tileBuffer) {
        tile.tri_indices_encapsulated.clear();
        tile.tri_indices_partial.clear();
        tile.dirty = false;
    }

    // Early exit if no triangles
    if (raster_triangles.empty()) return;

    const int tileSizeX = GetTileSizeX();
    const int tileSizeY = GetTileSizeY();
    const int tileCountX = GetTileCountX();
    const int tileCountY = GetTileCountY();
    const float invTileSizeX = 1.0f / tileSizeX;
    const float invTileSizeY = 1.0f / tileSizeY;

    for (int i = 0; i < static_cast<int>(raster_triangles.size()); i += 3) {
        const VERTEX& v1 = raster_triangles[i];
        const VERTEX& v2 = raster_triangles[i + 1];
        const VERTEX& v3 = raster_triangles[i + 2];
        
        const auto [minTriPt, maxTriPt] = MathUtil::ComputeBoundingBox(
            v1.GetXY(), v2.GetXY(), v3.GetXY()
        );
        
        int minTileX = std::max(0, static_cast<int>(minTriPt.x * invTileSizeX));
        int maxTileX = std::min(tileCountX - 1, static_cast<int>(maxTriPt.x * invTileSizeX + 0.999f));
        int minTileY = std::max(0, static_cast<int>(minTriPt.y * invTileSizeY));
        int maxTileY = std::min(tileCountY - 1, static_cast<int>(maxTriPt.y * invTileSizeY + 0.999f));

        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            const int rowOffset = ty * tileCountX;
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                const int tileIndex = rowOffset + tx;
                Tile& tile = _tileBuffer[tileIndex];

                if (DoesTileEncapsulate(tile, v1, v2, v3)) {
                    tile.tri_indices_encapsulated.push_back(i);
                } else {
                    tile.tri_indices_partial.push_back(i);
                }
            }
        }
    }
}

unsigned int TileManager::GetTileCountX() {
    return TILE_COUNT_X;
}

unsigned int TileManager::GetTileCountY() {
    return TILE_COUNT_Y;
}

unsigned int TileManager::GetTileSizeX() {
    return TILE_SIZE_X;
}

unsigned int TileManager::GetTileSizeY() {
    return TILE_SIZE_Y;
}

void TileManager::SetTileSize(const unsigned int x, const unsigned int y) {
    TILE_SIZE_X = x;
    TILE_SIZE_Y = y;
    InvalidateTiles();
}

void TileManager::CalculateTileCounts(unsigned int screen_width, unsigned int screen_height) {
    // Use ceiling division to ensure all pixels are covered by tiles
    TILE_COUNT_X = (screen_width + TILE_SIZE_X - 1) / TILE_SIZE_X;
    TILE_COUNT_Y = (screen_height + TILE_SIZE_Y - 1) / TILE_SIZE_Y;
}

void TileManager::InvalidateTiles() {
    _tilesInitialized = false;
}

bool TileManager::IsInitialized() const {
    return _tilesInitialized;
}

void TileManager::UpdateActiveTiles() {
    activeTiles.clear();
    for (auto& tile : _tileBuffer) {
        if (!tile.tri_indices_encapsulated.empty() || !tile.tri_indices_partial.empty()) {
            activeTiles.push_back(&tile);
        }
    }
}