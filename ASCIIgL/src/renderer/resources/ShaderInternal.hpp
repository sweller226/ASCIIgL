#pragma once

#include <memory>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <ASCIIgL/renderer/Shader.hpp>

namespace ASCIIgL {

class Shader::Impl {
public:
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
};

class ShaderProgram::Impl {
public:
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};

} // namespace ASCIIgL
