#pragma once

#include <unordered_map>
#include <memory>
#include <string>

#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/MonochromeMapping.hpp>

namespace ASCIIgL {

class TextureLibrary {
public:
    static TextureLibrary& GetInst() {
        static TextureLibrary inst;
        return inst;
    }
    std::shared_ptr<TextureArray> LoadTextureArray(const std::string& path, int tileSize, const std::string& savedName, const MonochromeMapping& mono=MonochromeMapping{});
    std::shared_ptr<Texture> LoadTexture(const std::string& path, const std::string& savedName, const MonochromeMapping& mono=MonochromeMapping{});

    std::shared_ptr<TextureArray> GetTextureArray(const std::string& savedName) const;
    std::shared_ptr<Texture> GetTexture(const std::string& savedName) const;
    bool HasTextureArray(const std::string& savedName) const;
    bool HasTexture(const std::string& savedName) const;

    // Direct accessors for iteration over internal maps.
    const std::unordered_map<std::string, std::shared_ptr<TextureArray>>& GetTextureArrays() const;
    const std::unordered_map<std::string, std::shared_ptr<Texture>>& GetTextures() const;

    void RemoveTextureArray(const std::string& savedName);
    void RemoveTexture(const std::string& savedName);
    void ClearTextureArrays();
    void ClearTextures();

private:
    TextureLibrary() = default;
    std::unordered_map<std::string, std::shared_ptr<TextureArray>> _textureArrays;
    std::unordered_map<std::string, std::shared_ptr<Texture>> _textures;
};

}