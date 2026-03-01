#include <ASCIIgL/renderer/Renderer.hpp>

#include <vector>
#include <cstring>     // std::memcpy
#include <sstream>     // std::ostringstream

#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

// =========================================================================
// Per-Mesh GPU Buffer Cache Management
// =========================================================================

Renderer::GPUMeshCache* Renderer::GetOrCreateMeshCache(const Mesh* mesh) {
    if (!mesh) return nullptr;
    
    // Check if cache already exists
    if (mesh->gpuBufferCache) {
        return static_cast<GPUMeshCache*>(mesh->gpuBufferCache);
    }
    
    // Create new cache
    GPUMeshCache* cache = new GPUMeshCache();
    
    // Create vertex buffer (immutable for static meshes)
    if (mesh->GetVertexDataSize() > 0) {
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = static_cast<UINT>(mesh->GetVertexDataSize());  // Size in bytes
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;  // Static - won't change
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        
        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = mesh->GetVertices().data();
        
        HRESULT hr = _device->CreateBuffer(&vbDesc, &vbData, &cache->vertexBuffer);
        if (FAILED(hr)) {
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[Renderer] Failed to create mesh vertex buffer: 0x" + ss.str());
            delete cache;
            return nullptr;
        }
        cache->vertexCount = mesh->GetVertexCount();
    }
    
    // Create index buffer if mesh is indexed
    if (!mesh->GetIndices().empty()) {
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(int) * mesh->GetIndices().size();
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        
        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = mesh->GetIndices().data();
        
        HRESULT hr = _device->CreateBuffer(&ibDesc, &ibData, &cache->indexBuffer);
        if (FAILED(hr)) {
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[Renderer] Failed to create mesh index buffer: 0x" + ss.str());
            delete cache;
            return nullptr;
        }
        cache->indexCount = mesh->GetIndices().size();
    }
    
    // Store cache in mesh
    mesh->gpuBufferCache = cache;
    
    Logger::Debug("[Renderer] Created GPU buffer cache for mesh: " + 
                  std::to_string(cache->vertexCount) + " vertices, " + 
                  std::to_string(cache->indexCount) + " indices");
    
    return cache;
}

void Renderer::ReleaseMeshCache(void* cachePtr) {
    if (!cachePtr) return;
    
    GPUMeshCache* cache = static_cast<GPUMeshCache*>(cachePtr);
    delete cache;  // ComPtr automatically releases buffers
}

void Renderer::InvalidateCachedTexture(const Texture* tex) {
    if (!tex) return;
    
    auto it = _textureCache.find(tex);
    if (it != _textureCache.end()) {
        _textureCache.erase(it);
        Logger::Debug("[Renderer] Texture cache invalidated for texture");
    }
}

// =========================================================================
// Texture Management
// =========================================================================

bool Renderer::CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv)
{
    if (!tex) return false;

    const int baseW = tex->GetWidth();
    const int baseH = tex->GetHeight();
    const uint8_t* baseData = tex->GetDataPtr();

    if (!baseData) return false;

    const bool hasCustom = tex->HasCustomMipmaps();
    const int mipCount = tex->GetMipCount();

    // -----------------------------
    // Texture data is already 0–255
    // -----------------------------
    auto GetTextureData = [&](const uint8_t* src, int w, int h) {
        size_t count = w * h * 4;
        std::vector<uint8_t> out(count);
        std::memcpy(out.data(), src, count);  // Data is already 0–255
        return out;
    };

    // -----------------------------
    // Create texture description
    // -----------------------------
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = baseW;
    desc.Height = baseH;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (hasCustom) {
        desc.MipLevels = mipCount;
        desc.MiscFlags = 0; // no auto-mips
    } else {
        desc.MipLevels = 0; // auto-generate
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create texture");
        return false;
    }

    // -----------------------------
    // Upload mip 0
    // -----------------------------
    {
        auto gpuData = GetTextureData(baseData, baseW, baseH);
        _context->UpdateSubresource(texture.Get(), 0, nullptr, gpuData.data(), baseW * 4, 0);
    }

    // -----------------------------
    // Upload custom mipmaps
    // -----------------------------
    if (hasCustom) {
        for (int level = 1; level < mipCount; ++level) {
            int w = tex->GetMipWidth(level);
            int h = tex->GetMipHeight(level);
            const uint8_t* mipData = tex->GetMipDataPtr(level);

            auto gpuData = GetTextureData(mipData, w, h);

            _context->UpdateSubresource(
                texture.Get(),
                level,
                nullptr,
                gpuData.data(),
                w * 4,
                0
            );
        }
    }

    // -----------------------------
    // Create SRV
    // -----------------------------
    hr = _device->CreateShaderResourceView(texture.Get(), nullptr, srv);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create SRV");
        return false;
    }

    // -----------------------------
    // Auto-generate GPU mipmaps if needed
    // -----------------------------
    if (!hasCustom) {
        _context->GenerateMips(*srv);
    }

    return true;
}

void Renderer::BindTexture(const Texture* tex, int slot) {
    if (!_initialized) {
        UnbindTexture(slot);
        return;
    }

    if (!tex) {
        UnbindTexture(slot);
        return;
    }

    // Check cache
    ID3D11ShaderResourceView* srv = nullptr;
    auto it = _textureCache.find(tex);
    if (it != _textureCache.end()) {
        srv = it->second.Get();
    } else {
        ComPtr<ID3D11ShaderResourceView> newSRV;
        if (!CreateTextureFromASCIIgLTexture(tex, &newSRV)) {
            UnbindTexture(slot);
            return;
        }
        srv = newSRV.Get();
        _textureCache[tex] = newSRV;
    }

    _context->PSSetShaderResources(slot, 1, &srv);
    _currentTextureSRV = srv;
}

void Renderer::UnbindTexture(int slot) {
    if (!_initialized) return;
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    _context->PSSetShaderResources(slot, 1, nullSRVs);
    if (slot == 0) _currentTextureSRV.Reset();
}

bool Renderer::CreateTextureArraySRV(const TextureArray* texArray, ID3D11ShaderResourceView** srv) {
    if (!texArray || !texArray->IsValid()) return false;

    const int layerCount = texArray->GetLayerCount();
    const int tileSize   = texArray->GetTileSize();
    const bool hasCustom = texArray->HasCustomMipmaps();
    const int mipCount   = texArray->GetMipCount();

    Logger::Debug("[Renderer] Creating Texture2DArray: " + std::to_string(layerCount) + " layers, " +
                  std::to_string(tileSize) + "x" + std::to_string(tileSize) + ", " +
                  std::to_string(mipCount) + " mip levels");

    // Create texture description for Texture2DArray
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = tileSize;
    desc.Height = tileSize;
    desc.ArraySize = layerCount;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (hasCustom) {
        desc.MipLevels = mipCount;
        desc.MiscFlags = 0;
    } else {
        desc.MipLevels = 0;
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create Texture2DArray");
        return false;
    }

    // Calculate actual mip levels for GPU texture
    int actualMipLevels = mipCount;
    if (!hasCustom) {
        int size = tileSize;
        actualMipLevels = 1;
        while (size > 1) {
            size /= 2;
            actualMipLevels++;
        }
    }

    Logger::Debug("[Renderer] Actual GPU mip levels: " + std::to_string(actualMipLevels));

    // Upload layers
    for (int layer = 0; layer < layerCount; ++layer) {
        if (hasCustom) {
            for (int mip = 0; mip < mipCount; ++mip) {
                const uint8_t* data = texArray->GetLayerData(layer, mip);
                if (!data) continue;

                int mipW = texArray->GetMipWidth(mip);
                int mipH = texArray->GetMipHeight(mip);

                UINT subresource = D3D11CalcSubresource(mip, layer, actualMipLevels);
                _context->UpdateSubresource(texture.Get(), subresource, nullptr, data, mipW * 4, 0);
            }
        } else {
            const uint8_t* data = texArray->GetLayerData(layer, 0);
            if (!data) continue;

            UINT subresource = D3D11CalcSubresource(0, layer, actualMipLevels);
            _context->UpdateSubresource(texture.Get(), subresource, nullptr, data, tileSize * 4, 0);
        }
    }

    // Create SRV for Texture2DArray (sRGB format so sampling returns linear RGB)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = hasCustom ? mipCount : -1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = layerCount;

    hr = _device->CreateShaderResourceView(texture.Get(), &srvDesc, srv);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create Texture2DArray SRV");
        return false;
    }

    if (!hasCustom) {
        _context->GenerateMips(*srv);
    }

    Logger::Info("[Renderer] Successfully created Texture2DArray");
    return true;
}

void Renderer::BindTextureArray(const TextureArray* texArray, int slot) {
    if (!_initialized || !texArray || !texArray->IsValid()) {
        UnbindTextureArray(slot);
        return;
    }

    ComPtr<ID3D11ShaderResourceView> srv;

    auto it = _textureArrayCache.find(texArray);
    if (it != _textureArrayCache.end()) {
        srv = it->second;
    } else {
        ID3D11ShaderResourceView* rawSRV = nullptr;
        if (CreateTextureArraySRV(texArray, &rawSRV)) {
            srv.Attach(rawSRV);
            _textureArrayCache[texArray] = srv;
        }
    }

    if (srv) {
        _currentTextureSRV = srv;
        ID3D11ShaderResourceView* srvs[] = { srv.Get() };
        _context->PSSetShaderResources(slot, 1, srvs);

        ID3D11SamplerState* samplers[] = { _samplerLinear.Get() };
        _context->PSSetSamplers(slot, 1, samplers);
    }
}

void Renderer::UnbindTextureArray(int slot) {
    if (!_initialized) return;
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    _context->PSSetShaderResources(slot, 1, nullSRVs);
}

} // namespace ASCIIgL

