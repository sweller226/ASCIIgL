#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/util/Logger.hpp>

#include <cstring>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION_ALREADY_DONE
#include <stb_image/stb_image.h>

namespace ASCIIgL {

// Forward declaration of BoxFilter from Texture.cpp
namespace MipFilters {
    void BoxFilter(const uint8_t* srcData, int srcW, int srcH, uint8_t* dstData, int dstW, int dstH);
}

// PIMPL Implementation
class TextureArray::Impl {
public:
    int tileSize = 0;
    int layerCount = 0;
    bool valid = false;
    bool hasCustomMipmaps = false;
    
    struct Layer {
        struct MipLevel {
            int width;
            int height;
            std::vector<uint8_t> data; // RGBA8
        };
        std::vector<MipLevel> mipChain;
    };
    
    std::vector<Layer> layers;
    
    // Load from atlas image
    bool LoadFromAtlas(const std::string& atlasPath, int tileSize);
    
    // Load from individual files
    bool LoadFromFiles(const std::vector<std::string>& tilePaths);
    
    // Generate mipmaps for all layers
    void GenerateMipmapsCPU(int maxLevels, MipFilters::MipFilterFn filter);
};

bool TextureArray::Impl::LoadFromAtlas(const std::string& atlasPath, int inTileSize) {
    Logger::Info("TEXTURE_ARRAY: Loading from atlas: " + atlasPath);
    
    stbi_set_flip_vertically_on_load(0);
    
    int atlasW, atlasH, bpp;
    stbi_uc* atlasData = stbi_load(atlasPath.c_str(), &atlasW, &atlasH, &bpp, 4);
    
    if (!atlasData) {
        Logger::Error("TEXTURE_ARRAY: Failed to load atlas: " + atlasPath);
        return false;
    }
    
    tileSize = inTileSize;
    const int tilesX = atlasW / tileSize;
    const int tilesY = atlasH / tileSize;
    layerCount = tilesX * tilesY;
    
    Logger::Debug("TEXTURE_ARRAY: Atlas " + std::to_string(atlasW) + "x" + std::to_string(atlasH) +
                  ", tiles: " + std::to_string(tilesX) + "x" + std::to_string(tilesY) +
                  " = " + std::to_string(layerCount) + " layers");
    
    layers.resize(layerCount);
    
    // Extract each tile as a layer
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            int layerIdx = ty * tilesX + tx;
            Layer& layer = layers[layerIdx];

            // Create base mip level
            layer.mipChain.resize(1);
            Layer::MipLevel& mip0 = layer.mipChain[0];
            mip0.width = tileSize;
            mip0.height = tileSize;
            mip0.data.resize(tileSize * tileSize * 4);
            
            // Copy tile data
            for (int y = 0; y < tileSize; ++y) {
                const int srcY = ty * tileSize + y;
                const uint8_t* srcRow = atlasData + (srcY * atlasW + tx * tileSize) * 4;
                uint8_t* dstRow = mip0.data.data() + y * tileSize * 4;
                std::memcpy(dstRow, srcRow, tileSize * 4);
            }
        }
    }
    
    stbi_image_free(atlasData);
    valid = true;
    
    Logger::Info("TEXTURE_ARRAY: Successfully loaded " + std::to_string(layerCount) + " layers");
    return true;
}

bool TextureArray::Impl::LoadFromFiles(const std::vector<std::string>& tilePaths) {
    if (tilePaths.empty()) {
        Logger::Error("TEXTURE_ARRAY: No tile paths provided");
        return false;
    }
    
    Logger::Info("TEXTURE_ARRAY: Loading " + std::to_string(tilePaths.size()) + " individual tiles");
    
    stbi_set_flip_vertically_on_load(0);
    
    layerCount = static_cast<int>(tilePaths.size());
    layers.resize(layerCount);
    tileSize = 0;
    
    for (int i = 0; i < layerCount; ++i) {
        int w, h, bpp;
        stbi_uc* data = stbi_load(tilePaths[i].c_str(), &w, &h, &bpp, 4);
        
        if (!data) {
            Logger::Error("TEXTURE_ARRAY: Failed to load tile: " + tilePaths[i]);
            return false;
        }
        
        if (tileSize == 0) {
            tileSize = w;
            if (w != h) {
                Logger::Error("TEXTURE_ARRAY: Tiles must be square, got " + std::to_string(w) + "x" + std::to_string(h));
                stbi_image_free(data);
                return false;
            }
        } else if (w != tileSize || h != tileSize) {
            Logger::Error("TEXTURE_ARRAY: All tiles must be same size. Expected " + 
                          std::to_string(tileSize) + ", got " + std::to_string(w));
            stbi_image_free(data);
            return false;
        }
        
        Layer& layer = layers[i];
        layer.mipChain.resize(1);
        Layer::MipLevel& mip0 = layer.mipChain[0];
        mip0.width = tileSize;
        mip0.height = tileSize;
        mip0.data.resize(tileSize * tileSize * 4);
        std::memcpy(mip0.data.data(), data, tileSize * tileSize * 4);
        
        stbi_image_free(data);
    }
    
    valid = true;
    Logger::Info("TEXTURE_ARRAY: Successfully loaded " + std::to_string(layerCount) + " layers");
    return true;
}

void TextureArray::Impl::GenerateMipmapsCPU(int maxLevels, MipFilters::MipFilterFn filter) {
    if (!valid || layers.empty()) return;
    
    if (!filter) {
        filter = MipFilters::BoxFilter;
    }
    
    // Calculate max possible mip levels
    int maxPossible = 0;
    int size = tileSize;
    while (size >= 1) {
        maxPossible++;
        size /= 2;
    }
    
    int targetLevels = (maxLevels < 0) ? maxPossible : std::min(maxLevels, maxPossible);
    
    Logger::Debug("TEXTURE_ARRAY: Generating " + std::to_string(targetLevels) + " mip levels for " +
                  std::to_string(layerCount) + " layers");
    
    for (Layer& layer : layers) {
        // Clear existing mips beyond level 0
        layer.mipChain.resize(1);
        
        int prevW = layer.mipChain[0].width;
        int prevH = layer.mipChain[0].height;
        
        for (int level = 1; level < targetLevels; ++level) {
            int newW = std::max(1, prevW / 2);
            int newH = std::max(1, prevH / 2);
            
            Layer::MipLevel newMip;
            newMip.width = newW;
            newMip.height = newH;
            newMip.data.resize(newW * newH * 4);
            
            const Layer::MipLevel& prevMip = layer.mipChain[level - 1];
            filter(prevMip.data.data(), prevW, prevH, newMip.data.data(), newW, newH);
            
            layer.mipChain.push_back(std::move(newMip));
            
            prevW = newW;
            prevH = newH;
        }
    }
    
    hasCustomMipmaps = true;
    Logger::Debug("TEXTURE_ARRAY: Mipmap generation complete");
}

// TextureArray public interface

TextureArray::TextureArray(const std::string& atlasPath, int tileSize)
    : pImpl(std::make_unique<Impl>())
{
    pImpl->LoadFromAtlas(atlasPath, tileSize);
}

TextureArray::TextureArray(const std::vector<std::string>& tilePaths)
    : pImpl(std::make_unique<Impl>())
{
    pImpl->LoadFromFiles(tilePaths);
}

TextureArray::~TextureArray() = default;

TextureArray::TextureArray(TextureArray&& other) noexcept = default;
TextureArray& TextureArray::operator=(TextureArray&& other) noexcept = default;

int TextureArray::GetTileSize() const { return pImpl->tileSize; }
int TextureArray::GetLayerCount() const { return pImpl->layerCount; }
int TextureArray::GetMipCount() const { 
    return pImpl->layers.empty() ? 0 : static_cast<int>(pImpl->layers[0].mipChain.size());
}
bool TextureArray::HasCustomMipmaps() const { return pImpl->hasCustomMipmaps; }
bool TextureArray::IsValid() const { return pImpl->valid; }

int TextureArray::GetMipWidth(int level) const {
    if (pImpl->layers.empty()) return 0;
    const auto& chain = pImpl->layers[0].mipChain;
    if (level < 0 || level >= static_cast<int>(chain.size())) return 0;
    return chain[level].width;
}

int TextureArray::GetMipHeight(int level) const {
    if (pImpl->layers.empty()) return 0;
    const auto& chain = pImpl->layers[0].mipChain;
    if (level < 0 || level >= static_cast<int>(chain.size())) return 0;
    return chain[level].height;
}

const uint8_t* TextureArray::GetLayerData(int layer, int mipLevel) const {
    if (layer < 0 || layer >= pImpl->layerCount) return nullptr;
    const auto& chain = pImpl->layers[layer].mipChain;
    if (mipLevel < 0 || mipLevel >= static_cast<int>(chain.size())) return nullptr;
    return chain[mipLevel].data.data();
}

void TextureArray::GenerateMipmapsCPU(int maxLevels, MipFilters::MipFilterFn filter) {
    pImpl->GenerateMipmapsCPU(maxLevels, filter);
}

} // namespace ASCIIgL
