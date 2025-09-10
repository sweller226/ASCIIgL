#include <ASCIIgL/engine/Mesh.hpp>

Mesh::Mesh(std::vector<VERTEX> inVerts, std::vector<Texture*> inTex)
	: vertices(inVerts), textures(inTex)
{

}

Mesh::~Mesh()
{
    for (size_t i = 0; i < textures.size(); i++)
    {
        delete textures[i]; // deleting heap memory
    }
    textures.clear();
}