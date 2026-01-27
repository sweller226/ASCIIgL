#pragma once

#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/engine/Texture.hpp>

#include <vector>
#include <cstddef>

namespace ASCIIgL {

// Forward declaration
class RendererGPU;

class Mesh // this class represents a mesh with generic vertex data and a vertex format descriptor
{
	friend class RendererGPU;
	friend class RendererCPU;
private:
	std::vector<std::byte> vertexData;  // Raw vertex data as bytes
	std::vector<int> indices;
	VertFormat format;                // Describes the vertex layout
	Texture* texture; // single texture pointer (not owned by Mesh)

	// GPU buffer cache (opaque pointer - managed by RendererGPU)
	// This allows RendererGPU to cache buffers without Mesh knowing about DirectX
	mutable void* gpuBufferCache = nullptr;

public:

	// Constructor with indices
	Mesh(std::vector<std::byte>&& inVertexData, const VertFormat& inFormat, std::vector<int>&& inIndices, Texture* inTex)
		: vertexData(std::move(inVertexData))
		, indices(std::move(inIndices))
		, format(inFormat)
		, texture(inTex) {}

	// Constructor without indices
	Mesh(std::vector<std::byte>&& inVertexData, const VertFormat& inFormat, Texture* inTex)
		: vertexData(std::move(inVertexData))
		, format(inFormat)
		, texture(inTex) {}

	~Mesh();

	// Immutable access to mesh data
	const std::vector<std::byte>& GetVertices() const { return vertexData; }  // Full vector with size
	const std::vector<int>& GetIndices() const { return indices; }
	const VertFormat& GetVertFormat() const { return format; }
	const Texture* GetTexture() const { return texture; }
	size_t GetVertexCount() const { return vertexData.size() / format.GetStride(); }
	size_t GetIndexCount() const { return indices.size(); }
	size_t GetVertexDataSize() const { return vertexData.size(); }
	bool IsIndexed() const { return !indices.empty(); }

	void ReleaseGpuCache();
};
} // namespace ASCIIgL