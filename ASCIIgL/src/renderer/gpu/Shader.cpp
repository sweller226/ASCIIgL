#include <ASCIIgL/renderer/gpu/Shader.hpp>

#include <iostream>

#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>

#include <ASCIIgL/util/Logger.hpp>


namespace ASCIIgL {

// =========================================================================
// UniformDescriptor Implementation
// =========================================================================

UniformDescriptor::UniformDescriptor(const std::string& name, UniformType type, uint32_t offset)
    : name(name)
    , type(type)
    , offset(offset)
    , size(GetTypeSize(type))
{
}

uint32_t UniformDescriptor::GetTypeSize(UniformType type) {
    switch (type) {
        case UniformType::Float:   return 4;
        case UniformType::Float2:  return 8;
        case UniformType::Float3:  return 12;
        case UniformType::Float4:  return 16;
        case UniformType::Int:     return 4;
        case UniformType::Int2:    return 8;
        case UniformType::Int3:    return 12;
        case UniformType::Int4:    return 16;
        case UniformType::Mat3:    return 48;  // 3x4 floats (HLSL packing)
        case UniformType::Mat4:    return 64;
        default:                   return 0;
    }
}

uint32_t UniformDescriptor::GetTypeAlignment(UniformType type) {
    // HLSL constant buffer packing rules:
    // - float, int: 4-byte aligned
    // - float2, int2: 8-byte aligned
    // - float3, float4, int3, int4: 16-byte aligned
    // - matrices: 16-byte aligned (each row starts on 16-byte boundary)
    switch (type) {
        case UniformType::Float:
        case UniformType::Int:     return 4;
        case UniformType::Float2:
        case UniformType::Int2:    return 8;
        case UniformType::Float3:
        case UniformType::Float4:
        case UniformType::Int3:
        case UniformType::Int4:
        case UniformType::Mat3:
        case UniformType::Mat4:    return 16;
        default:                   return 16;
    }
}

// =========================================================================
// UniformBufferLayout Implementation
// =========================================================================

UniformBufferLayout::Builder& UniformBufferLayout::Builder::Add(const std::string& name, UniformType type) {
    // Apply HLSL alignment rules
    uint32_t alignment = UniformDescriptor::GetTypeAlignment(type);
    uint32_t size = UniformDescriptor::GetTypeSize(type);
    
    // Align current offset
    _currentOffset = (_currentOffset + alignment - 1) & ~(alignment - 1);
    
    // Special handling for types that can't cross 16-byte boundaries
    if (type == UniformType::Float3 || type == UniformType::Int3) {
        // float3 uses 16 bytes in constant buffers
        size = 16;
    }
    
    _uniforms.emplace_back(name, type, _currentOffset);
    _currentOffset += size;
    
    return *this;
}

UniformBufferLayout UniformBufferLayout::Builder::Build() const {
    UniformBufferLayout layout;
    layout._uniforms = _uniforms;
    
    // Total size must be multiple of 16 bytes (HLSL constant buffer requirement)
    layout._size = (_currentOffset + 15) & ~15;
    
    return layout;
}

bool UniformBufferLayout::HasUniform(const std::string& name) const {
    return GetUniform(name) != nullptr;
}

const UniformDescriptor* UniformBufferLayout::GetUniform(const std::string& name) const {
    for (const auto& uniform : _uniforms) {
        if (uniform.name == name) {
            return &uniform;
        }
    }
    return nullptr;
}

// =========================================================================
// Shader Implementation
// =========================================================================

Shader::Shader(ShaderType type)
    : _type(type)
{
}

Shader::~Shader() = default;

std::unique_ptr<Shader> Shader::CreateFromSource(
    const std::string& source,
    ShaderType type,
    const std::string& entryPoint)
{
    auto shader = std::unique_ptr<Shader>(new Shader(type));
    
    if (!shader->CompileFromSource(source, entryPoint)) {
        Logger::Error("Failed to compile shader: " + shader->_compileError);
        // Still return the shader so caller can check the error
    }
    
    return shader;
}

std::unique_ptr<Shader> Shader::CreateFromBytecode(
    const std::vector<uint8_t>& bytecode,
    ShaderType type)
{
    auto shader = std::unique_ptr<Shader>(new Shader(type));
    
    if (!shader->LoadFromBytecode(bytecode)) {
        Logger::Error("Failed to load shader bytecode");
    }
    
    return shader;
}

#ifdef _WIN32

bool Shader::CompileFromSource(const std::string& source, const std::string& entryPoint) {
    const char* target = (_type == ShaderType::Vertex) ? "vs_5_0" : "ps_5_0";
    
    ID3DBlob* errorBlob = nullptr;
    ID3DBlob* shaderBlob = nullptr;
    
    HRESULT hr = D3DCompile(
        source.c_str(),
        source.length(),
        nullptr,
        nullptr,
        nullptr,
        entryPoint.c_str(),
        target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG,
        0,
        &shaderBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            _compileError = std::string(static_cast<char*>(errorBlob->GetBufferPointer()));
            errorBlob->Release();
        } else {
            _compileError = "Unknown compilation error";
        }
        return false;
    }

    if (errorBlob) {
        // Warnings
        Logger::Warning("Shader compilation warnings: " + 
                       std::string(static_cast<char*>(errorBlob->GetBufferPointer())));
        errorBlob->Release();
    }

    _bytecode.Attach(shaderBlob);
    
    // Get the D3D device from RendererGPU
    auto& gpu = RendererGPU::GetInst();
    if (!gpu._device) {
        _compileError = "RendererGPU device not initialized";
        return false;
    }

    if (_type == ShaderType::Vertex) {
        hr = gpu._device->CreateVertexShader(
            _bytecode->GetBufferPointer(),
            _bytecode->GetBufferSize(),
            nullptr,
            &_vertexShader
        );
    } else {
        hr = gpu._device->CreatePixelShader(
            _bytecode->GetBufferPointer(),
            _bytecode->GetBufferSize(),
            nullptr,
            &_pixelShader
        );
    }

    if (FAILED(hr)) {
        _compileError = "Failed to create shader object";
        return false;
    }

    _isValid = true;
    return true;
}

bool Shader::LoadFromBytecode(const std::vector<uint8_t>& bytecode) {
    // Create a blob from the bytecode
    HRESULT hr = D3DCreateBlob(bytecode.size(), &_bytecode);
    if (FAILED(hr)) {
        _compileError = "Failed to create blob for bytecode";
        return false;
    }
    
    memcpy(_bytecode->GetBufferPointer(), bytecode.data(), bytecode.size());
    
    // Get the D3D device from RendererGPU
    auto& gpu = RendererGPU::GetInst();
    if (!gpu._device) {
        _compileError = "RendererGPU device not initialized";
        return false;
    }

    if (_type == ShaderType::Vertex) {
        hr = gpu._device->CreateVertexShader(
            _bytecode->GetBufferPointer(),
            _bytecode->GetBufferSize(),
            nullptr,
            &_vertexShader
        );
    } else {
        hr = gpu._device->CreatePixelShader(
            _bytecode->GetBufferPointer(),
            _bytecode->GetBufferSize(),
            nullptr,
            &_pixelShader
        );
    }

    if (FAILED(hr)) {
        _compileError = "Failed to create shader from bytecode";
        return false;
    }

    _isValid = true;
    return true;
}

#else
// Stub implementations for non-Windows platforms
bool Shader::CompileFromSource(const std::string&, const std::string&) {
    _compileError = "Shaders not supported on this platform";
    return false;
}

bool Shader::LoadFromBytecode(const std::vector<uint8_t>&) {
    _compileError = "Shaders not supported on this platform";
    return false;
}
#endif

// =========================================================================
// ShaderProgram Implementation
// =========================================================================

ShaderProgram::ShaderProgram() = default;
ShaderProgram::~ShaderProgram() = default;

std::unique_ptr<ShaderProgram> ShaderProgram::Create(
    std::unique_ptr<Shader> vertexShader,
    std::unique_ptr<Shader> pixelShader,
    const VertFormat& vertexFormat,
    const UniformBufferLayout& uniformLayout)
{
    auto program = std::unique_ptr<ShaderProgram>(new ShaderProgram());
    
    if (!program->Initialize(std::move(vertexShader), std::move(pixelShader), 
                            vertexFormat, uniformLayout)) {
        Logger::Error("Failed to create shader program: " + program->_error);
    }
    
    return program;
}

bool ShaderProgram::Initialize(
    std::unique_ptr<Shader> vertexShader,
    std::unique_ptr<Shader> pixelShader,
    const VertFormat& vertexFormat,
    const UniformBufferLayout& uniformLayout)
{
    if (!vertexShader || !vertexShader->IsValid()) {
        _error = "Invalid vertex shader";
        if (vertexShader) {
            _error += ": " + vertexShader->GetCompileError();
        }
        return false;
    }
    
    if (!pixelShader || !pixelShader->IsValid()) {
        _error = "Invalid pixel shader";
        if (pixelShader) {
            _error += ": " + pixelShader->GetCompileError();
        }
        return false;
    }
    
    if (vertexShader->GetType() != ShaderType::Vertex) {
        _error = "First shader must be a vertex shader";
        return false;
    }
    
    if (pixelShader->GetType() != ShaderType::Pixel) {
        _error = "Second shader must be a pixel shader";
        return false;
    }
    
    _vertexShader = std::move(vertexShader);
    _pixelShader = std::move(pixelShader);
    _vertexFormat = vertexFormat;
    _uniformLayout = uniformLayout;
    
    if (!CreateInputLayout(vertexFormat)) {
        return false;
    }
    
    _isValid = true;
    return true;
}

#ifdef _WIN32

bool ShaderProgram::CreateInputLayout(const VertFormat& format) {
    auto& gpu = RendererGPU::GetInst();
    if (!gpu._device) {
        _error = "RendererGPU device not initialized";
        return false;
    }

    const auto& elements = format.GetElements();
    std::vector<D3D11_INPUT_ELEMENT_DESC> inputDesc;
    inputDesc.reserve(elements.size());

    for (const auto& element : elements) {
        D3D11_INPUT_ELEMENT_DESC desc = {};
        desc.SemanticName = GetSemanticName(element.GetSemantic());
        desc.SemanticIndex = element.GetSemanticIndex();
        desc.Format = GetDXGIFormat(element.GetType());
        desc.InputSlot = 0;
        desc.AlignedByteOffset = element.GetOffset();
        desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        desc.InstanceDataStepRate = 0;
        
        inputDesc.push_back(desc);
    }

    HRESULT hr = gpu._device->CreateInputLayout(
        inputDesc.data(),
        static_cast<UINT>(inputDesc.size()),
        _vertexShader->_bytecode->GetBufferPointer(),
        _vertexShader->_bytecode->GetBufferSize(),
        &_inputLayout
    );

    if (FAILED(hr)) {
        _error = "Failed to create input layout";
        return false;
    }

    return true;
}

#else
bool ShaderProgram::CreateInputLayout(const VertFormat&) {
    _error = "Shader programs not supported on this platform";
    return false;
}
#endif

// =========================================================================
// Default Shader Sources
// =========================================================================

namespace DefaultShaders {

const char* GetDefaultVertexShaderSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.texcoord = input.texcoord;
    return output;
}
)";
}

const char* GetDefaultPixelShaderSource() {
    return R"(
Texture2D diffuseTexture : register(t0);
SamplerState samplerState : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return diffuseTexture.Sample(samplerState, input.texcoord);
}
)";
}

const char* GetUnlitColorVertexShaderSource() {
    return R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}
)";
}

const char* GetUnlitColorPixelShaderSource() {
    return R"(
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
)";
}

UniformBufferLayout GetDefaultUniformLayout() {
    return UniformBufferLayout::Builder()
        .Add("mvp", UniformType::Mat4)
        .Build();
}

} // namespace DefaultShaders

} // namespace ASCIIgL
