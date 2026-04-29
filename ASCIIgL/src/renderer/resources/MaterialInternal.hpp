#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <ASCIIgL/renderer/Material.hpp>

namespace ASCIIgL {

class Material::Impl {
public:
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    bool constantBufferInitialized = false;
};

} // namespace ASCIIgL
