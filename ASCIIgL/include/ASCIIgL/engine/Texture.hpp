#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <memory>
#include <functional>

#include <ASCIIgL/engine/MipFilters.hpp>
#include <ASCIIgL/engine/MonochromeMapping.hpp>

namespace ASCIIgL {

class Texture // this class loads a texture from a path and holds its data in a 1d buffer
{
public:
	std::string texType;

	// type is for model drawing, as they have texture types.
	// Optional monochrome mapping can bake the texture to a gradient on load.
	Texture(const std::string& path, std::string type = "NULL",
	        const MonochromeMapping& mono = MonochromeMapping{});
	~Texture();

	// Move constructor and assignment for PIMPL
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	// Delete copy constructor and assignment (or implement if needed)
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	// getters for width and height
	int GetWidth() const;
	int GetHeight() const;
	std::string GetFilePath() const;
	
	// returns RGB color as vec3 with values between 0 and 1
	glm::vec3 GetPixelRGB(int x, int y) const;
	glm::vec4 GetPixelRGBA(int x, int y) const;
	
	// Fast accessor - returns pointer to raw uint8_t data (RGBA)
	const uint8_t* GetPixelRGBAPtr(int x, int y) const;
	const uint8_t* GetDataPtr() const;

	// Mipmap info
	int GetMipCount() const;
	bool HasCustomMipmaps() const;

	// Access specific mip level
	int GetMipWidth(int level) const;
	int GetMipHeight(int level) const;
	const uint8_t* GetMipDataPtr(int level) const;

	// Add custom mipmaps (user-supplied)
	void AddCustomMipLevel(int width, int height, const std::vector<uint8_t>& data);

	// CPU generation
	// User-defined mip filter function:
	// srcData: previous level RGBA (0–255), size = srcW * srcH * 4
	// dstData: next level RGBA (0–255), size = dstW * dstH * 4
	void GenerateMipmapsCPU(int maxLevels = -1, MipFilters::MipFilterFn filter = nullptr); // -1 = generate until 1x1

	// Replace base mip level with new data (used for atlas padding expansion)
	void ReplaceBaseLevel(int newWidth, int newHeight, std::vector<uint8_t>&& newData);

private:
	// Forward declaration for PIMPL pattern
	class Impl;
	std::unique_ptr<Impl> pImpl;
};

} // namespace ASCIIgL