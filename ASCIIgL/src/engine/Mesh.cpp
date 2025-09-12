#include <ASCIIgL/engine/Mesh.hpp>

Mesh::Mesh(std::vector<VERTEX>&& inVerts, Texture* inTex)
	: vertices(std::move(inVerts)), texture(inTex)
{

}

Mesh::~Mesh()
{
    // Note: We don't delete the texture here as it might be shared
    // The texture should be managed by the owner (e.g., Model class)
}