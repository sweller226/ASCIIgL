#include <ASCIIgL/engine/Texture.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <stb_image/stb_image.h>
#include <algorithm>

namespace ASCIIgL {

static void BoxFilter(
    const uint8_t* srcData, int srcW, int srcH,
    uint8_t* dstData, int dstW, int dstH)
{
    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {

            int sx = x * 2;
            int sy = y * 2;

            auto sample = [&](int px, int py) {
                px = std::min(srcW - 1, px);
                py = std::min(srcH - 1, py);
                size_t idx = (py * srcW + px) * 4;
                return &srcData[idx];
            };

            const uint8_t* p00 = sample(sx,     sy);
            const uint8_t* p10 = sample(sx + 1, sy);
            const uint8_t* p01 = sample(sx,     sy + 1);
            const uint8_t* p11 = sample(sx + 1, sy + 1);

            size_t di = (y * dstW + x) * 4;

            for (int c = 0; c < 4; ++c) {
                dstData[di + c] =
                    (p00[c] + p10[c] + p01[c] + p11[c]) / 4;
            }
        }
    }
}

// PIMPL Implementation class that contains all stb_image-related code
class Texture::Impl {
public:
    int _width, _height, _bpp;
    std::string FilePath;

    struct MipLevel {
        int width;
        int height;
        std::vector<uint8_t> data; // RGBA8 in your 0–15 format
    };

    std::vector<MipLevel> mipChain; // mipChain[0] = base level
    bool hasCustomMipmaps = false;

    Impl(const std::string& path);
    ~Impl() = default;

    glm::vec3 GetPixelRGB(int x, int y) const;
    glm::vec4 GetPixelRGBA(int x, int y) const;
    inline const uint8_t* GetPixelRGBAPtr(int x, int y) const;

    int GetMipCount() const;
    bool HasCustomMipmaps() const;
    void AddCustomMipLevel(int w, int h, const std::vector<uint8_t>& d);

    void GenerateMipmapsCPU(int maxLevels, MipFilterFn filter = BoxFilter);
};

Texture::Impl::Impl(const std::string& path)
    : FilePath(path), _width(0), _height(0), _bpp(0)
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
        return;
    }

    // Convert from 0-255 to 0-15 range and store in mipChain[0]
    const size_t bufferSize = _width * _height * 4;

    MipLevel base;
    base.width = _width;
    base.height = _height;
    base.data.resize(bufferSize);

    for (size_t i = 0; i < bufferSize; ++i) {
        // Convert 0-255 to 0-15: (value * 15 + 127) / 255
        base.data[i] = (tempRGBABuffer[i] * 15 + 127) / 255;
    }

    mipChain.clear();
    mipChain.push_back(std::move(base));

    // Free the temporary buffer
    stbi_image_free(tempRGBABuffer);

    Logger::Info("TEXTURE: Successfully loaded '" + path + "'");
    Logger::Debug("TEXTURE: Dimensions: " + std::to_string(_width) + "x" + std::to_string(_height));
    Logger::Debug("TEXTURE: Original channels: " + std::to_string(_bpp));
    Logger::Debug("TEXTURE: RGBA buffer size: " + std::to_string(bufferSize) + " bytes (0-15 range)");
}

glm::vec3 Texture::Impl::GetPixelRGB(int x, int y) const {
    const MipLevel& base = mipChain[0];
    const size_t offset = (static_cast<size_t>(y) * base.width + static_cast<size_t>(x)) * 4;

    return glm::vec3(
        base.data[offset],
        base.data[offset + 1],
        base.data[offset + 2]
    );
}

glm::vec4 Texture::Impl::GetPixelRGBA(int x, int y) const {
    const MipLevel& base = mipChain[0];
    const size_t offset = (static_cast<size_t>(y) * base.width + static_cast<size_t>(x)) * 4;

    return glm::vec4(
        base.data[offset],
        base.data[offset + 1],
        base.data[offset + 2],
        base.data[offset + 3]
    );
}

inline const uint8_t* Texture::Impl::GetPixelRGBAPtr(int x, int y) const {
    const MipLevel& base = mipChain[0];
    const size_t offset = (static_cast<size_t>(y) * base.width + static_cast<size_t>(x)) * 4;
    return &base.data[offset];
}

int Texture::Impl::GetMipCount() const { 
    return (int)mipChain.size(); 
}
bool Texture::Impl::HasCustomMipmaps() const { 
    return hasCustomMipmaps; 
}

void Texture::Impl::AddCustomMipLevel(int w, int h, const std::vector<uint8_t>& d) {
    mipChain.push_back({ w, h, d });
    hasCustomMipmaps = true;
}   

void Texture::Impl::GenerateMipmapsCPU(int maxLevels, MipFilterFn filter) {
    if (mipChain.empty()) return;

    // Clear any previously generated mips (keep only base level)
    mipChain.resize(1);
    hasCustomMipmaps = false;

    int w = mipChain[0].width;
    int h = mipChain[0].height;

    // Compute maximum possible levels
    int maxPossible = 1;
    int tw = w, th = h;
    while (tw > 1 || th > 1) {
        tw = std::max(1, tw / 2);
        th = std::max(1, th / 2);
        maxPossible++;
    }

    if (maxLevels < 0 || maxLevels > maxPossible)
        maxLevels = maxPossible;

    mipChain.reserve(maxLevels);

    // Use default BoxFilter if none provided
    if (!filter)
        filter = BoxFilter;

    for (int level = 1; level < maxLevels; ++level) {
        const MipLevel& prev = mipChain[level - 1];

        int newW = std::max(1, prev.width / 2);
        int newH = std::max(1, prev.height / 2);

        MipLevel next;
        next.width = newW;
        next.height = newH;
        next.data.resize(newW * newH * 4);

        // Call user-supplied or default filter
        filter(prev.data.data(), prev.width, prev.height,
               next.data.data(), newW, newH);

        mipChain.push_back(std::move(next));
    }
}

// Texture public interface implementation
Texture::Texture(const std::string& path, const std::string type)
    : texType(type), pImpl(std::make_unique<Impl>(path)) { }

Texture::~Texture() = default;

Texture::Texture(Texture&& other) noexcept
    : texType(std::move(other.texType)), pImpl(std::move(other.pImpl)) { }

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        texType = std::move(other.texType);
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

int Texture::GetWidth() const {
    return pImpl->_width;
}

int Texture::GetHeight() const {
    return pImpl->_height;
}

std::string Texture::GetFilePath() const {
    return pImpl->FilePath;
}

glm::vec3 Texture::GetPixelRGB(int x, int y) const {
    return pImpl->GetPixelRGB(x, y);
}

glm::vec4 Texture::GetPixelRGBA(int x, int y) const {
    return pImpl->GetPixelRGBA(x, y);
}

const uint8_t* Texture::GetPixelRGBAPtr(int x, int y) const {
    return pImpl->GetPixelRGBAPtr(x, y);
}

const uint8_t* Texture::GetDataPtr() const {
    // Return pointer to mip 0 data for compatibility
    if (pImpl->mipChain.empty()) return nullptr;
    return pImpl->mipChain[0].data.data();
}

int Texture::GetMipCount() const { return pImpl->GetMipCount(); }
bool Texture::HasCustomMipmaps() const { return pImpl->HasCustomMipmaps(); }

int Texture::GetMipWidth(int level) const {
    if (level < 0 || level >= static_cast<int>(pImpl->mipChain.size())) return 0;
    return pImpl->mipChain[level].width;
}

int Texture::GetMipHeight(int level) const {
    if (level < 0 || level >= static_cast<int>(pImpl->mipChain.size())) return 0;
    return pImpl->mipChain[level].height;
}

const uint8_t* Texture::GetMipDataPtr(int level) const {
    if (level < 0 || level >= static_cast<int>(pImpl->mipChain.size())) return nullptr;
    return pImpl->mipChain[level].data.data();
}

void Texture::AddCustomMipLevel(int w, int h, const std::vector<uint8_t>& d) {
    pImpl->AddCustomMipLevel(w, h, d);
}

void Texture::GenerateMipmapsCPU(int maxLevels, MipFilterFn filter) {
    pImpl->GenerateMipmapsCPU(maxLevels, filter);
}

} // namespace ASCIIgL