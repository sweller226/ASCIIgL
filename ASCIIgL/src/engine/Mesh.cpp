#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/RendererGPU.hpp>

Mesh::Mesh(std::vector<VERTEX>&& inVerts, std::vector<int>&& inIndices, Texture* inTex)
	: vertices(std::move(inVerts)), indices(std::move(inIndices)), texture(inTex)
{

}

Mesh::Mesh(std::vector<VERTEX>&& inVerts, Texture* inTex)
	: vertices(std::move(inVerts)), texture(inTex)
{

}

Mesh::~Mesh()
{
    // Cleanup GPU buffer cache if it exists
    if (gpuBufferCache && RendererGPU::GetInst().IsInitialized()) {
        RendererGPU::GetInst().ReleaseMeshCache(gpuBufferCache);
        gpuBufferCache = nullptr;
    }
    
    // Note: We don't delete the texture here as it might be shared
    // The texture should be managed by the owner (e.g., Model class)
}