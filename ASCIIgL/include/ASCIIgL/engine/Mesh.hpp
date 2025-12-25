#pragma once

#include <ASCIIgL/renderer/Vertex.hpp>
#include <ASCIIgL/engine/Texture.hpp>

#include <vector>

// Forward declaration
class RendererGPU;

class Mesh // this class is a mesh the represents a group of vertices along with a group of textures to render them with
{
	friend class RendererGPU;
	friend class RendererCPU;
private:
	std::vector<VERTEX> vertices;
	std::vector<int> indices;

	// GPU buffer cache (opaque pointer - managed by RendererGPU)
	// This allows RendererGPU to cache buffers without Mesh knowing about DirectX
	mutable void* gpuBufferCache = nullptr;

public:
	Texture* texture; // single texture pointer (not owned by Mesh)

	Mesh(std::vector<VERTEX>&& inVerts, std::vector<int>&& inIndices, Texture* inTex);
	Mesh(std::vector<VERTEX>&& inVerts, Texture* inTex);
	~Mesh();

	// Immutable access to mesh data
	const std::vector<VERTEX>& GetVertices() const { return vertices; }
	const std::vector<int>& GetIndices() const { return indices; }
	size_t GetVertexCount() const { return vertices.size(); }
	size_t GetIndexCount() const { return indices.size(); }
	bool IsIndexed() const { return !indices.empty(); }

};