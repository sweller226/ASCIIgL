#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/renderer/Renderer.hpp>

namespace ASCIIgL {

Mesh::~Mesh()
{
    // Cleanup GPU buffer cache if it exists
    if (gpuBufferCache && Renderer::GetInst().IsInitialized()) {
        Renderer::GetInst().ReleaseMeshCache(gpuBufferCache);
        gpuBufferCache = nullptr;
    }
    
    // Note: We don't delete the texture here as it might be shared
    // The texture should be managed by the owner (e.g., Model class)
}

void Mesh::ReleaseGpuCache() {
    if (gpuBufferCache && Renderer::GetInst().IsInitialized()) {
        Renderer::GetInst().ReleaseMeshCache(gpuBufferCache);
        gpuBufferCache = nullptr;
    }
}

} // namespace ASCIIgL