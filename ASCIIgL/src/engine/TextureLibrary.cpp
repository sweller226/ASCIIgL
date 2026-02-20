#include <ASCIIgL/engine/TextureLibrary.hpp>

#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

std::shared_ptr<TextureArray> TextureLibrary::LoadTextureArray(const std::string& path, int tileSize, const std::string& savedName)
{
    // Already loaded?
    std::string nameToSave = savedName;
    if (nameToSave == "")
        nameToSave = path;
    
    auto it = _textureArrays.find(nameToSave);
    if (it != _textureArrays.end())
        return it->second;

    // Load new texture array
    auto texArray = std::make_shared<TextureArray>(path, tileSize);
    if (!texArray || !texArray->IsValid()) {
        Logger::Error("[TextureLibrary] Failed to load TextureArray: " + path);
        return nullptr;
    }

    _textureArrays[nameToSave] = texArray;
    return texArray;
}

std::shared_ptr<Texture> TextureLibrary::LoadTexture(const std::string& path, const std::string& savedName)
{
    // Already loaded?
    std::string nameToSave = savedName;
    if (nameToSave == "")
        nameToSave = path;
    
    auto it = _textures.find(nameToSave);
    if (it != _textures.end())
        return it->second;

    // Load new texture
    auto tex = std::make_shared<Texture>(path);
    if (!tex || tex->GetWidth() == 0 || tex->GetHeight() == 0) {
        Logger::Error("[TextureLibrary] Failed to load Texture: " + path);
        return nullptr;
    }

    _textures[nameToSave] = tex;
    return tex;
}

std::shared_ptr<TextureArray> TextureLibrary::GetTextureArray(const std::string& savedName) const
{
    auto it = _textureArrays.find(savedName);
    return (it != _textureArrays.end()) ? it->second : nullptr;
}

std::shared_ptr<Texture> TextureLibrary::GetTexture(const std::string& savedName) const
{
    auto it = _textures.find(savedName);
    return (it != _textures.end()) ? it->second : nullptr;
}

bool TextureLibrary::HasTextureArray(const std::string& savedName) const
{
    return _textureArrays.find(savedName) != _textureArrays.end();
}

bool TextureLibrary::HasTexture(const std::string& savedName) const
{
    return _textures.find(savedName) != _textures.end();
}

void TextureLibrary::RemoveTextureArray(const std::string& savedName)
{
    _textureArrays.erase(savedName);
}

void TextureLibrary::RemoveTexture(const std::string& savedName)
{
    _textures.erase(savedName);
}

void TextureLibrary::ClearTextureArrays()
{
    _textureArrays.clear();
}

void TextureLibrary::ClearTextures()
{
    _textures.clear();
}

} // namespace ASCIIgL