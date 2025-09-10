#include <ASCIIgL/engine/Texture.hpp>

#include <ASCIIgL/engine/Logger.hpp>
#include <stb_image/stb_image.h>
#include <algorithm>

// PIMPL Implementation class that contains all stb_image-related code
class Texture::Impl
{
public:
    stbi_uc* m_RGBBuffer;       // buffer that holds RGB color data (3 channels)
    stbi_uc* m_GrayscaleBuffer; // buffer that holds grayscale data (1 channel)
    int m_Width, m_Height, m_BPP;
    std::string FilePath;

    Impl(const std::string& path);
    ~Impl();
    float GetPixelGrayscale(int x, int y) const;
    glm::vec3 GetPixelRGB(int x, int y) const;
};

Texture::Impl::Impl(const std::string& path)
    : FilePath(path), m_RGBBuffer(nullptr), m_GrayscaleBuffer(nullptr), m_Width(0), m_Height(0), m_BPP(0)
{
    Logger::Info("TEXTURE: Attempting to load texture: " + path);
    
    stbi_set_flip_vertically_on_load(0);
    
    // Load RGB version (3 channels)
    m_RGBBuffer = stbi_load(path.c_str(), &m_Width, &m_Height, &m_BPP, 3);
    
    // Load grayscale version (1 channel)
    int grayWidth, grayHeight, grayBPP;
    m_GrayscaleBuffer = stbi_load(path.c_str(), &grayWidth, &grayHeight, &grayBPP, 1);
    
    // Verify if texture loaded successfully
    if (m_RGBBuffer && m_GrayscaleBuffer) {
        Logger::Info("TEXTURE: Successfully loaded '" + path + "'");
        Logger::Debug("TEXTURE: Dimensions: " + std::to_string(m_Width) + "x" + std::to_string(m_Height));
        Logger::Debug("TEXTURE: Original channels: " + std::to_string(m_BPP));
        Logger::Debug("TEXTURE: RGB buffer size: " + std::to_string(m_Width * m_Height * 3) + " bytes");
        Logger::Debug("TEXTURE: Grayscale buffer size: " + std::to_string(m_Width * m_Height) + " bytes");
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
    if (m_RGBBuffer) {
        stbi_image_free(m_RGBBuffer);
    }
    if (m_GrayscaleBuffer) {
        stbi_image_free(m_GrayscaleBuffer);
    }
}

float Texture::Impl::GetPixelGrayscale(int x, int y) const
{
    if (!m_GrayscaleBuffer) return 0.0f;
    
    // Use integer math for offset calculation and unsigned char for pixel data
    const size_t offset = static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x);
    const unsigned char* buffer = reinterpret_cast<const unsigned char*>(m_GrayscaleBuffer);
    unsigned int shade = buffer[offset];

    constexpr float inv255 = 1.0f / 255.0f;
    return shade * inv255;
}

glm::vec3 Texture::Impl::GetPixelRGB(int x, int y) const
{
    if (!m_RGBBuffer) return glm::vec3(0.0f);
    
    // Use integer math for offset calculation - RGB has 3 channels
    const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x)) * 3;
    const unsigned char* buffer = reinterpret_cast<const unsigned char*>(m_RGBBuffer);
    
    constexpr float inv255 = 1.0f / 255.0f;
    return glm::vec3(
        buffer[offset] * inv255,     // Red
        buffer[offset + 1] * inv255, // Green
        buffer[offset + 2] * inv255  // Blue
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
    return pImpl->m_Width;
}

int Texture::GetHeight() const
{
    return pImpl->m_Height;
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