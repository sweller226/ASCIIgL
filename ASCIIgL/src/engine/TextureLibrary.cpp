#include <ASCIIgL/engine/TextureLibrary.hpp>

#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

std::shared_ptr<TextureArray> TextureLibrary::LoadTextureArray(const std::string& path, int tileSize, const std::string& savedName, const MonochromeMapping& mono)
{
    // Already loaded?
    std::string nameToSave = savedName.empty() ? path : savedName;

    auto it = _textureArrays.find(nameToSave);
    if (it != _textureArrays.end())
        return it->second;

    auto texArray = std::make_shared<TextureArray>(path, tileSize, mono);
    if (!texArray || !texArray->IsValid()) {
        Logger::Error("[TextureLibrary] Failed to load TextureArray: " + path);
        return nullptr;
    }

    _textureArrays[nameToSave] = texArray;
    return texArray;
}

std::shared_ptr<Texture> TextureLibrary::LoadTexture(const std::string& path, const std::string& savedName, const MonochromeMapping& mono)
{
    // If already loaded under this name, drop the old entry so we can reload.
    std::string nameToSave = savedName.empty() ? path : savedName;

    auto it = _textures.find(nameToSave);
    if (it != _textures.end()) {
        _textures.erase(it);
    }

    auto tex = std::make_shared<Texture>(path, "NULL", mono);
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

const std::unordered_map<std::string, std::shared_ptr<TextureArray>>& TextureLibrary::GetTextureArrays() const {
    return _textureArrays;
}

const std::unordered_map<std::string, std::shared_ptr<Texture>>& TextureLibrary::GetTextures() const {
    return _textures;
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