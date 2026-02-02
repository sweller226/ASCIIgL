#include <ASCIIgL/engine/TextureLibrary.hpp>

#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

std::shared_ptr<TextureArray> TextureLibrary::LoadTextureArray(const std::string& path, int tileSize)
{
    // Already loaded?
    auto it = _textureArrays.find(path);
    if (it != _textureArrays.end())
        return it->second;

    // Load new texture array
    auto texArray = std::make_shared<TextureArray>(path, tileSize);
    if (!texArray || !texArray->IsValid()) {
        Logger::Error("[TextureLibrary] Failed to load TextureArray: " + path);
        return nullptr;
    }

    _textureArrays[path] = texArray;
    return texArray;
}

std::shared_ptr<Texture> TextureLibrary::LoadTexture(const std::string& path)
{
    // Already loaded?
    auto it = _textures.find(path);
    if (it != _textures.end())
        return it->second;

    // Load new texture
    auto tex = std::make_shared<Texture>(path);
    if (!tex || tex->GetWidth() == 0 || tex->GetHeight() == 0) {
        Logger::Error("[TextureLibrary] Failed to load Texture: " + path);
        return nullptr;
    }

    _textures[path] = tex;
    return tex;
}

std::shared_ptr<TextureArray> TextureLibrary::GetTextureArray(const std::string& path) const
{
    auto it = _textureArrays.find(path);
    return (it != _textureArrays.end()) ? it->second : nullptr;
}

std::shared_ptr<Texture> TextureLibrary::GetTexture(const std::string& path) const
{
    auto it = _textures.find(path);
    return (it != _textures.end()) ? it->second : nullptr;
}

bool TextureLibrary::HasTextureArray(const std::string& path) const
{
    return _textureArrays.find(path) != _textureArrays.end();
}

bool TextureLibrary::HasTexture(const std::string& path) const
{
    return _textures.find(path) != _textures.end();
}

void TextureLibrary::RemoveTextureArray(const std::string& path)
{
    _textureArrays.erase(path);
}

void TextureLibrary::RemoveTexture(const std::string& path)
{
    _textures.erase(path);
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