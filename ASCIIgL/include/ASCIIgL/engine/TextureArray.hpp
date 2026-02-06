#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <ASCIIgL/engine/MipFilters.hpp>

namespace ASCIIgL {

// TextureArray - Manages a Texture2DArray with per-layer mipmaps
// Used for block textures to avoid atlas bleeding issues
class TextureArray {
public:
    // Load from existing atlas image (splits into tiles)
    // atlasPath: path to terrain.png or similar atlas
    // tileSize: size of each tile (e.g., 16 for 16x16 tiles)
    TextureArray(const std::string& atlasPath, int tileSize);
    
    // Load from individual tile files
    TextureArray(const std::vector<std::string>& tilePaths);
    
    ~TextureArray();
    
    // Move semantics (PIMPL pattern)
    TextureArray(TextureArray&& other) noexcept;
    TextureArray& operator=(TextureArray&& other) noexcept;
    
    // No copy
    TextureArray(const TextureArray&) = delete;
    TextureArray& operator=(const TextureArray&) = delete;
    
    // Getters
    int GetTileSize() const;
    int GetLayerCount() const;
    int GetMipCount() const;
    bool HasCustomMipmaps() const;
    bool IsValid() const;
    
    // Access layer data
    int GetMipWidth(int level) const;
    int GetMipHeight(int level) const;
    const uint8_t* GetLayerData(int layer, int mipLevel = 0) const;
    
    // Generate CPU mipmaps for all layers
    void GenerateMipmapsCPU(int maxLevels = -1, MipFilters::MipFilterFn filter = nullptr);

    static int GetLayerFromAtlasXY(int x, int y, int atlasSize);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace ASCIIgL
