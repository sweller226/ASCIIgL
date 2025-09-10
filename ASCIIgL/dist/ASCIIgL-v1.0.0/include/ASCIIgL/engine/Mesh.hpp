#pragma once

#include <ASCIIgL/renderer/Vertex.hpp>
#include <ASCIIgL/engine/Texture.hpp>

#include <vector>

class Mesh // this class is a mesh the represents a group of vertices along with a group of textures to render them with
{
private:

public:
	std::vector<VERTEX> vertices;
	std::vector<Texture*> textures; // puts textures on heap because they break otherwise

	Mesh(std::vector<VERTEX> inVerts, std::vector<Texture*> inTex);
	~Mesh();

};