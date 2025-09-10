#include <ASCIIgL/engine/Texture.hpp>

#include <ASCIIgL/engine/Logger.hpp>
#include <stb_image/stb_image.h>
#include <algorithm>

// PIMPL Implementation class that contains all stb_image-related code
class Texture::Impl
{
public:
    stbi_uc* m_LocalBuffer; // buffer that holds all of the colour data
    int m_Width, m_Height, m_BPP;
    std::string FilePath;

    Impl(const std::string& path);
    ~Impl();
    float GetPixelCol(int x, int y) const;
};

Texture::Impl::Impl(const std::string& path)
    : FilePath(path), m_LocalBuffer(nullptr), m_Width(0), m_Height(0), m_BPP(0)
{
    Logger::Info("TEXTURE: Attempting to load texture: " + path);
    
    stbi_set_flip_vertically_on_load(0);
    m_LocalBuffer = stbi_load(path.c_str(), &m_Width, &m_Height, &m_BPP, 1);
    
    // Verify if texture loaded successfully
    if (m_LocalBuffer) {
        Logger::Info("TEXTURE: Successfully loaded '" + path + "'");
        Logger::Debug("TEXTURE: Dimensions: " + std::to_string(m_Width) + "x" + std::to_string(m_Height));
        Logger::Debug("TEXTURE: Channels: " + std::to_string(m_BPP) + " (forced to 1 for grayscale)");
        Logger::Debug("TEXTURE: Buffer size: " + std::to_string(m_Width * m_Height) + " bytes");
    } else {
        const char* stbi_error = stbi_failure_reason();
        Logger::Error("TEXTURE: Failed to load '" + path + "'");
        Logger::Error("TEXTURE: STB Error: " + std::string(stbi_error ? stbi_error : "Unknown error"));
        Logger::Error("TEXTURE: Check if file exists and is a valid image format");
    }
}

Texture::Impl::~Impl()
{
    // deletes the buffer
    if (m_LocalBuffer) {
        stbi_image_free(m_LocalBuffer);
    }
}

float Texture::Impl::GetPixelCol(int x, int y) const
{
    // Use integer math for offset calculation and unsigned char for pixel data
    const size_t offset = static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x);
    const unsigned char* buffer = reinterpret_cast<unsigned char*>(m_LocalBuffer);
    unsigned int shade = buffer[offset];

    constexpr float inv255 = 1.0f / 255.0f;
    return shade * inv255;
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

float Texture::GetPixelCol(int x, int y) const
{
    return pImpl->GetPixelCol(x, y);
}