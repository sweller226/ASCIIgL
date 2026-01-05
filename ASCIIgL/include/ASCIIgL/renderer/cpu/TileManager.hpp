#pragma once

#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

struct Tile {
    glm::vec2 position;
    glm::vec2 size;
    std::vector<int> tri_indices_encapsulated;
    std::vector<int> tri_indices_partial;

    bool dirty = false;
};

class TileManager {
public:
    static TileManager& GetInst() {
        static TileManager instance;
        return instance;
    }

    std::vector<Tile> _tileBuffer;
    std::vector<Tile*> activeTiles;

    bool IsInitialized() const;
    
    unsigned int GetTileCountX();
    unsigned int GetTileCountY();
    unsigned int GetTileSizeX();
    unsigned int GetTileSizeY();
    
    void SetTileSize(unsigned int size_x, unsigned int size_y);
    void InitializeTiles();
    void BinTrianglesToTiles(const std::vector<VertStructs::PosWUVInvW>& raster_triangles);
    
    void UpdateActiveTiles();
    
    // Delete copy/move constructors and assignment
    TileManager(const TileManager&) = delete;
    TileManager& operator=(const TileManager&) = delete;
    TileManager(TileManager&&) = delete;
    TileManager& operator=(TileManager&&) = delete;
    
    private:
    TileManager() = default;
    ~TileManager() = default;
    
    void ClearTileTriangleLists();
    bool DoesTileEncapsulate(const Tile& tile, const VertStructs::PosWUVInvW& vert1, const VertStructs::PosWUVInvW& vert2, const VertStructs::PosWUVInvW& vert3);
    void InvalidateTiles();
    void CalculateTileCounts();
    void BinTrianglesToTilesSingleThreaded(const std::vector<VertStructs::PosWUVInvW>& raster_triangles);
    void BinTrianglesToTilesMultiThreaded(const std::vector<VertStructs::PosWUVInvW>& raster_triangles);

    // Tile properties
    unsigned int TILE_COUNT_X = 0;
    unsigned int TILE_COUNT_Y = 0;
    unsigned int TILE_SIZE_X = 16;
    unsigned int TILE_SIZE_Y = 16;

    bool _tilesInitialized = false;
};

} // namespace ASCIIgL