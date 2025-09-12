#pragma once

#include <ASCIIgL/renderer/Vertex.hpp>
#include <ASCIIgL/engine/Texture.hpp>

#include <vector>

class Mesh // this class is a mesh the represents a group of vertices along with a group of textures to render them with
{
private:

public:
	std::vector<VERTEX> vertices;
	Texture* texture; // single texture pointer

	Mesh(std::vector<VERTEX>&& inVerts, Texture* inTex);
	~Mesh();

};