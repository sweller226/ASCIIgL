#include <ASCIIgL/engine/Texture.hpp>

#include <ASCIIgL/engine/Logger.hpp>
#include <stb_image/stb_image.h>
#include <algorithm>

// PIMPL Implementation class that contains all stb_image-related code
class Texture::Impl
{
public:
    float* _RGBBuffer;       // buffer that holds RGB color data (3 channels)
    float* _GrayscaleBuffer; // buffer that holds grayscale data (1 channel)
    int _width, _height, _bpp;
    std::string FilePath;

    Impl(const std::string& path);
    ~Impl();
    float GetPixelGrayscale(int x, int y) const;
    glm::vec3 GetPixelRGB(int x, int y) const;
};

Texture::Impl::Impl(const std::string& path)
    : FilePath(path), _RGBBuffer(nullptr), _GrayscaleBuffer(nullptr), _width(0), _height(0), _bpp(0)
{
    Logger::Info("TEXTURE: Attempting to load texture: " + path);
    
    stbi_set_flip_vertically_on_load(0);
    
    // Load RGB version (3 channels)
    stbi_uc* tempRGBBuffer = stbi_load(path.c_str(), &_width, &_height, &_bpp, 3);
        _RGBBuffer = new float[_width * _height * 3];
    for (int i = 0; i < _width * _height * 3; ++i) { _RGBBuffer[i] = tempRGBBuffer[i] / 255.0f; }
    stbi_image_free(tempRGBBuffer);
    
    
    // Load grayscale version (1 channel)
    int grayWidth, grayHeight, grayBPP;
    stbi_uc* tempGrayscaleBuffer = stbi_load(path.c_str(), &grayWidth, &grayHeight, &grayBPP, 1);
        _GrayscaleBuffer = new float[grayWidth * grayHeight];
    for (int i = 0; i < _width * _height; ++i) { _GrayscaleBuffer[i] = tempGrayscaleBuffer[i] / 255.0f; }
    stbi_image_free(tempGrayscaleBuffer);

    // Verify if texture loaded successfully
    if (_RGBBuffer && _GrayscaleBuffer) {
        Logger::Info("TEXTURE: Successfully loaded '" + path + "'");
        Logger::Debug("TEXTURE: Dimensions: " + std::to_string(_width) + "x" + std::to_string(_height));
        Logger::Debug("TEXTURE: Original channels: " + std::to_string(_bpp));
        Logger::Debug("TEXTURE: RGB buffer size: " + std::to_string(_width * _height * 3) + " bytes");
        Logger::Debug("TEXTURE: Grayscale buffer size: " + std::to_string(_width * _height) + " bytes");
    } else {
        const char* stbi_error = stbi_failure_reason();
        Logger::Error("TEXTURE: Failed to load '" + path + "'");
        Logger::Error("TEXTURE: STB Error: " + std::string(stbi_error ? stbi_error : "Unknown error"));
        Logger::Error("TEXTURE: Check if file exists and is a valid image format");
    }
}

Texture::Impl::~Impl()
{
    // deletes the buffers
    if (_RGBBuffer) {
        delete[] _RGBBuffer;
    }
    if (_GrayscaleBuffer) {
        delete[] _GrayscaleBuffer;
    }
}

float Texture::Impl::GetPixelGrayscale(int x, int y) const
{
    if (!_GrayscaleBuffer) return 0.0f;
    const size_t offset = static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x);
        return _GrayscaleBuffer[offset];
}

glm::vec3 Texture::Impl::GetPixelRGB(int x, int y) const
{
    if (!_RGBBuffer) return glm::vec3(0.0f);

    const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x)) * 3;
    return glm::vec3(
        _RGBBuffer[offset],     // Red
        _RGBBuffer[offset + 1], // Green
        _RGBBuffer[offset + 2]  // Blue
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

float Texture::GetPixelGrayscale(int x, int y) const
{
    return pImpl->GetPixelGrayscale(x, y);
}

glm::vec3 Texture::GetPixelRGB(int x, int y) const
{
    return pImpl->GetPixelRGB(x, y);
}