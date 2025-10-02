#include <ASCIIgL/engine/Texture.hpp>

#include <ASCIIgL/engine/Logger.hpp>
#include <stb_image/stb_image.h>
#include <algorithm>

// PIMPL Implementation class that contains all stb_image-related code
class Texture::Impl
{
public:
    float* _RGBABuffer;       // buffer that holds RGBA color data (4 channels)
    int _width, _height, _bpp;
    std::string FilePath;

    Impl(const std::string& path);
    ~Impl();
    glm::vec3 GetPixelRGB(int x, int y) const;
    glm::vec4 GetPixelRGBA(int x, int y) const;
};

Texture::Impl::Impl(const std::string& path)
    : FilePath(path), _RGBABuffer(nullptr), _width(0), _height(0), _bpp(0)
{
    Logger::Info("TEXTURE: Attempting to load texture: " + path);

    stbi_set_flip_vertically_on_load(0);

    // Load RGBA version (4 channels)
    stbi_uc* tempRGBABuffer = stbi_load(path.c_str(), &_width, &_height, &_bpp, 4);

    if (!tempRGBABuffer) {
        const char* stbi_error = stbi_failure_reason();
        Logger::Error("TEXTURE: Failed to load '" + path + "'");
        Logger::Error("TEXTURE: STB Error: " + std::string(stbi_error ? stbi_error : "Unknown error"));
        Logger::Error("TEXTURE: Check if file exists and is a valid image format");
        _width = _height = _bpp = 0;
        _RGBABuffer = nullptr;
        return;
    }

    _RGBABuffer = new float[_width * _height * 4];
    for (int i = 0; i < _width * _height * 4; ++i) {
        _RGBABuffer[i] = tempRGBABuffer[i] / 255.0f;
    }
    stbi_image_free(tempRGBABuffer);

    Logger::Info("TEXTURE: Successfully loaded '" + path + "'");
    Logger::Debug("TEXTURE: Dimensions: " + std::to_string(_width) + "x" + std::to_string(_height));
    Logger::Debug("TEXTURE: Original channels: " + std::to_string(_bpp));
    Logger::Debug("TEXTURE: RGBA buffer size: " + std::to_string(_width * _height * 4) + " bytes");
}

Texture::Impl::~Impl()
{
    // deletes the buffers
    if (_RGBABuffer) {
        delete[] _RGBABuffer;
    }
}

glm::vec3 Texture::Impl::GetPixelRGB(int x, int y) const
{
    if (!_RGBABuffer) return glm::vec3(0.0f);
    
    // Clamp coordinates to valid range
    x = std::clamp(x, 0, _width - 1);
    y = std::clamp(y, 0, _height - 1);

    const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x)) * 4;
    return glm::vec3(
        _RGBABuffer[offset],     // Red
        _RGBABuffer[offset + 1], // Green
        _RGBABuffer[offset + 2]  // Blue
    );
}

glm::vec4 Texture::Impl::GetPixelRGBA(int x, int y) const
{
    if (!_RGBABuffer) return glm::vec4(0.0f);
    
    // Clamp coordinates to valid range
    x = std::clamp(x, 0, _width - 1);
    y = std::clamp(y, 0, _height - 1);

    const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x)) * 4;
    return glm::vec4(
        _RGBABuffer[offset],     // Red
        _RGBABuffer[offset + 1], // Green
        _RGBABuffer[offset + 2], // Blue
        _RGBABuffer[offset + 3]  // Alpha
    );
}

// Texture public interface implementation
Texture::Texture(const std::string& path, const std::string type)
    : texType(type), pImpl(std::make_unique<Impl>(path))
{
}

Texture::~Texture() = default;

Texture::Texture(Texture&& other) noexcept 
    : texType(std::move(other.texType)), pImpl(std::move(other.pImpl))
{
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        texType = std::move(other.texType);
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

int Texture::GetWidth() const
{
    return pImpl->_width;
}

int Texture::GetHeight() const
{
    return pImpl->_height;
}

std::string Texture::GetFilePath() const
{
    return pImpl->FilePath;
}

glm::vec3 Texture::GetPixelRGB(int x, int y) const
{
    return pImpl->GetPixelRGB(x, y);
}

glm::vec4 Texture::GetPixelRGBA(int x, int y) const
{
    return pImpl->GetPixelRGBA(x, y);
}