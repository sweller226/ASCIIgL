#pragma once

#include <cstring>

#include <d3dcompiler.h>

#include <ASCIIgL/renderer/Shader.hpp>

namespace ASCIIgL {

class ShaderIncludeHandler : public ID3DInclude {
public:
    explicit ShaderIncludeHandler(const ShaderIncludeMap* map)
        : _map(map) {
    }

    HRESULT STDMETHODCALLTYPE Open(
        D3D_INCLUDE_TYPE /*includeType*/,
        LPCSTR fileName,
        LPCVOID /*parentData*/,
        LPCVOID* data,
        UINT* bytes) override {
        if (!_map || !data || !bytes) {
            return E_INVALIDARG;
        }

        auto it = _map->find(fileName);
        if (it == _map->end()) {
            const char* base = std::strrchr(fileName, '/');
            if (!base) {
                base = std::strrchr(fileName, '\\');
            }
            const char* name = base ? base + 1 : fileName;
            it = _map->find(name);
        }

        if (it == _map->end()) {
            return E_FAIL;
        }

        *data = it->second.data();
        *bytes = static_cast<UINT>(it->second.size());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Close(LPCVOID /*data*/) override {
        return S_OK;
    }

private:
    const ShaderIncludeMap* _map;
};

} // namespace ASCIIgL
