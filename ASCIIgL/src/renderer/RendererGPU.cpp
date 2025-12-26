#include <ASCIIgL/renderer/RendererGPU.hpp>

#include <iostream>
#include <Windows.h>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/Screen.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/MathUtil.hpp>

namespace ASCIIgL {

// =========================================================================
// Default HLSL Shader Sources
// =========================================================================

static const char* g_VertexShaderSource = R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct VS_INPUT
{
    float4 position : POSITION;   // XYZW from VERTEX data[0-3]
    float3 texcoord : TEXCOORD0;  // UVW from VERTEX data[4-6]
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    
    // Transform position to clip space
    output.position = mul(mvp, input.position);
    
    // Pass through texture coordinates (just UV, ignore W for now)
    output.texcoord = input.texcoord.xy;
    
    return output;
}
)";

static const char* g_PixelShaderSource = R"(
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

Texture2D diffuseTexture : register(t0);
SamplerState samplerState : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Sample texture
    float4 color = diffuseTexture.Sample(samplerState, input.texcoord);
    return color;
}
)";

// =========================================================================
// Forward Declarations for Static Helper Functions
// =========================================================================
static bool CompileShaderFromSource(ID3D11Device* device, const std::string& source, 
                                    const char* entryPoint, const char* target, ID3DBlob** blob);
static bool CreateVertexShaderFromSource(ID3D11Device* device, const std::string& source, 
                                         const char* entryPoint, ID3D11VertexShader** vertexShader,
                                         ID3D11InputLayout** inputLayout);
static bool CreatePixelShaderFromSource(ID3D11Device* device, const std::string& source, 
                                        const char* entryPoint, ID3D11PixelShader** pixelShader);

// =========================================================================
// Destructor
// =========================================================================

RendererGPU::~RendererGPU() {
    Shutdown();
}

// =========================================================================
// Initialization
// =========================================================================

void RendererGPU::Initialize() {
    if (!_initialized) {
        Logger::Info("Initializing RendererGPU...");
    } else {
        Logger::Warning("RendererGPU is already initialized!");
        return;
    }

    if (!Screen::GetInst().IsInitialized()) {
        Logger::Error("Renderer: Screen must be initialized before creating Renderer.");
        throw std::runtime_error("Renderer: Screen must be initialized before creating Renderer.");
    }

    if (!_renderer) { // intialization should only be called from renderer, but just in case
        _renderer = &Renderer::GetInst();
    }

    std::cout << "[RendererGPU] Initializing DirectX 11..." << std::endl;

    // Initialize constant buffer data with defaults
    _currentConstantBuffer.mvp = glm::mat4(1.0f);

    if (!InitializeDevice()) {
        std::cerr << "[RendererGPU] Failed to initialize device" << std::endl;
        return;
    }

    if (!InitializeRenderTarget()) {
        std::cerr << "[RendererGPU] Failed to initialize render target" << std::endl;
        return;
    }

    if (!InitializeDepthStencil()) {
        std::cerr << "[RendererGPU] Failed to initialize depth stencil" << std::endl;
        return;
    }

    if (!InitializeShaders()) {
        std::cerr << "[RendererGPU] Failed to initialize shaders" << std::endl;
        return;
    }

    if (!InitializeBuffers()) {
        std::cerr << "[RendererGPU] Failed to initialize buffers" << std::endl;
        return;
    }

    if (!InitializeSamplers()) {
        std::cerr << "[RendererGPU] Failed to initialize samplers" << std::endl;
        return;
    }

    if (!InitializeRasterizerStates()) {
        std::cerr << "[RendererGPU] Failed to initialize rasterizer states" << std::endl;
        return;
    }

    if (!InitializeStagingTexture()) {
        std::cerr << "[RendererGPU] Failed to initialize staging texture" << std::endl;
        return;
    }

    if (!InitializeDebugSwapChain()) {
        std::cerr << "[RendererGPU] Failed to initialize debug swap chain (non-fatal)" << std::endl;
        // Non-fatal - continue without swap chain
    }

    _initialized = true;
    std::cout << "[RendererGPU] DirectX 11 initialized successfully" << std::endl;
    Logger::Debug("RendererGPU initialization complete - Device, Shaders, Buffers ready");
}

bool RendererGPU::InitializeDevice() {
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Use hardware acceleration
        nullptr,                    // No software module
        createDeviceFlags,          // Device flags
        featureLevels,              // Feature levels to try
        ARRAYSIZE(featureLevels),   // Number of feature levels
        D3D11_SDK_VERSION,          // SDK version
        &_device,                   // Output device
        &featureLevel,              // Chosen feature level
        &_context                   // Output context
    );

    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] D3D11CreateDevice failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    std::cout << "[RendererGPU] Device created with feature level: " 
              << (featureLevel == D3D_FEATURE_LEVEL_11_1 ? "11.1" : "11.0") << std::endl;

    return true;
}

bool RendererGPU::InitializeRenderTarget() {
    // Get MSAA settings
    bool useMSAA = Renderer::GetInst().GetAntialiasing();
    int msaaSamples = Renderer::GetInst().GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    UINT qualityLevel = 0;
    
    if (useMSAA) {
        sampleCount = std::max(1, std::min(8, msaaSamples));  // Clamp to 1-8 samples
        
        // Check MSAA quality support
        UINT numQualityLevels = 0;
        _device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, sampleCount, &numQualityLevels);
        if (numQualityLevels > 0) {
            qualityLevel = 0;  // Use highest quality
        } else {
            std::cerr << "[RendererGPU] " << sampleCount << "x MSAA not supported, falling back to 1x" << std::endl;
            sampleCount = 1;
        }
    }
    
    Logger::Debug("[RendererGPU] Creating render target with " + std::to_string(sampleCount) + "x MSAA");
    
    // Create MSAA render target texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = Screen::GetInst().GetWidth();
    texDesc.Height = Screen::GetInst().GetHeight();
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = sampleCount;
    texDesc.SampleDesc.Quality = qualityLevel;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;

    HRESULT hr = _device->CreateTexture2D(&texDesc, nullptr, &_renderTarget);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create render target texture: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Create render target view
    hr = _device->CreateRenderTargetView(_renderTarget.Get(), nullptr, &_renderTargetView);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create render target view: 0x" << std::hex << hr << std::endl;
        return false;
    }
    
    // Create resolved (non-MSAA) texture for CPU download
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.BindFlags = 0;  // No binding needed, just for copying
    
    hr = _device->CreateTexture2D(&texDesc, nullptr, &_resolvedTexture);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create resolved texture: 0x" << std::hex << hr << std::endl;
        return false;
    }

    std::cout << "[RendererGPU] Render target created: " 
              << Screen::GetInst().GetWidth() << "x" << Screen::GetInst().GetHeight() << std::endl;

    return true;
}

bool RendererGPU::InitializeDepthStencil() {
    // Get MSAA settings (must match render target)
    bool useMSAA = Renderer::GetInst().GetAntialiasing();
    int msaaSamples = Renderer::GetInst().GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    if (useMSAA) {
        sampleCount = std::max(1, std::min(8, msaaSamples));
    }
    
    // Create depth stencil texture (must match render target MSAA settings)
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = Screen::GetInst().GetWidth();
    depthDesc.Height = Screen::GetInst().GetHeight();
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = sampleCount;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = _device->CreateTexture2D(&depthDesc, nullptr, &_depthStencilBuffer);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create depth stencil buffer: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Create depth stencil view
    hr = _device->CreateDepthStencilView(_depthStencilBuffer.Get(), nullptr, &_depthStencilView);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create depth stencil view: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Create depth stencil state
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;  // Standard depth test: closer objects win
    dsDesc.StencilEnable = FALSE;

    hr = _device->CreateDepthStencilState(&dsDesc, &_depthStencilState);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create depth stencil state: 0x" << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

bool RendererGPU::InitializeShaders() {
    // Compile and create vertex shader
    if (!CreateVertexShaderFromSource(_device.Get(), g_VertexShaderSource, "main", &_vertexShader, &_inputLayout)) {
        return false;
    }

    // Compile and create pixel shader
    if (!CreatePixelShaderFromSource(_device.Get(), g_PixelShaderSource, "main", &_pixelShader)) {
        return false;
    }

    std::cout << "[RendererGPU] Shaders compiled successfully" << std::endl;
    return true;
}

bool RendererGPU::InitializeBuffers() {
    // Create constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = _device->CreateBuffer(&cbDesc, nullptr, &_constantBuffer);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create constant buffer: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Initial vertex buffer (will resize dynamically)
    _vertexBufferCapacity = INITIAL_VERTEX_BUFFER_CAPACITY;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = static_cast<UINT>(_vertexBufferCapacity);  // Capacity is already in bytes
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = _device->CreateBuffer(&vbDesc, nullptr, &_vertexBuffer);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create vertex buffer: 0x" << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

bool RendererGPU::InitializeSamplers() {
    // Create point sampler with linear mipmap blending for pixel art
    // Point filtering keeps pixels sharp, linear mip blending reduces shimmering at distance
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;  // Point filtering with smooth mipmap transitions
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;  // Clamp to prevent bleeding at edges
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MipLODBias = -0.5f;  // Use slightly sharper mipmaps to reduce atlas bleeding
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = _device->CreateSamplerState(&samplerDesc, &_samplerLinear);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create sampler state: 0x" << std::hex << hr << std::endl;
        return false;
    }

    Logger::Debug("[RendererGPU] Created point sampler with linear mipmap blending for pixel art");
    return true;
}

bool RendererGPU::InitializeRasterizerStates() {
    D3D11_RASTERIZER_DESC desc = {};
    desc.DepthClipEnable = TRUE;  // Enable depth clipping

    // Create all 8 combinations: wireframe(0/1) + cull(0/2) + ccw(0/4)
    for (int i = 0; i < 8; ++i) {
        bool wireframe = (i & 1) != 0;      // Bit 0: wireframe
        bool cull = (i & 2) != 0;           // Bit 1: backface culling
        bool ccw = (i & 4) != 0;            // Bit 2: counter-clockwise winding

        desc.FillMode = wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
        desc.CullMode = cull ? D3D11_CULL_BACK : D3D11_CULL_NONE;
        desc.FrontCounterClockwise = ccw ? TRUE : FALSE;

        HRESULT hr = _device->CreateRasterizerState(&desc, &_rasterizerStates[i]);
        if (FAILED(hr)) {
            std::cerr << "[RendererGPU] Failed to create rasterizer state " << i << ": 0x" << std::hex << hr << std::endl;
            return false;
        }
    }

    return true;
}

bool RendererGPU::InitializeStagingTexture() {
    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = Screen::GetInst().GetWidth();
    stagingDesc.Height = Screen::GetInst().GetHeight();
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = _device->CreateTexture2D(&stagingDesc, nullptr, &_stagingTexture);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create staging texture: 0x" << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

// =========================================================================
// Shader Compilation (Internal - Default Shaders Only)
// =========================================================================

static bool CompileShaderFromSource(ID3D11Device* device, const std::string& source, 
                                    const char* entryPoint, const char* target, ID3DBlob** blob) {
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompile(
        source.c_str(),
        source.length(),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        blob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "[RendererGPU] Shader compilation failed:\n" 
                      << (char*)errorBlob->GetBufferPointer() << std::endl;
            errorBlob->Release();
        }
        return false;
    }

    if (errorBlob) errorBlob->Release();
    return true;
}

static bool CreateVertexShaderFromSource(ID3D11Device* device, const std::string& source, 
                                         const char* entryPoint, ID3D11VertexShader** vertexShader,
                                         ID3D11InputLayout** inputLayout) {
    ComPtr<ID3DBlob> vsBlob;
    if (!CompileShaderFromSource(device, source, entryPoint, "vs_5_0", &vsBlob)) {
        return false;
    }

    HRESULT hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        vertexShader
    );

    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create vertex shader: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Create input layout to match VERTEX structure (7 floats = 28 bytes)
    // data[0-3] = XYZW (position, 16 bytes)
    // data[4-6] = UVW (texcoord, 12 bytes)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },  // XYZW
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },  // UVW
    };

    hr = device->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        inputLayout
    );

    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create input layout: 0x" << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

static bool CreatePixelShaderFromSource(ID3D11Device* device, const std::string& source, 
                                        const char* entryPoint, ID3D11PixelShader** pixelShader) {
    ComPtr<ID3DBlob> psBlob;
    if (!CompileShaderFromSource(device, source, entryPoint, "ps_5_0", &psBlob)) {
        return false;
    }

    HRESULT hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        pixelShader
    );

    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create pixel shader: 0x" << std::hex << hr << std::endl;
        return false;
    }

    return true;
}

// =========================================================================
// Buffer Management
// =========================================================================

bool RendererGPU::UpdateVertexBuffer(const std::vector<std::byte>& vertices) {
    if (vertices.empty()) return true;

    // Resize buffer if needed (capacity is in bytes)
    if (vertices.size() > _vertexBufferCapacity) {
        _vertexBufferCapacity = vertices.size() * BUFFER_GROWTH_FACTOR;

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = static_cast<UINT>(_vertexBufferCapacity);
        vbDesc.Usage = D3D11_USAGE_DYNAMIC;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _vertexBuffer.Reset();
        HRESULT hr = _device->CreateBuffer(&vbDesc, nullptr, &_vertexBuffer);
        if (FAILED(hr)) {
            std::cerr << "[RendererGPU] Failed to resize vertex buffer: 0x" << std::hex << hr << std::endl;
            return false;
        }
    }

    // Map and update
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to map vertex buffer: 0x" << std::hex << hr << std::endl;
        return false;
    }

    memcpy(mapped.pData, vertices.data(), vertices.size());
    _context->Unmap(_vertexBuffer.Get(), 0);

    return true;
}

bool RendererGPU::UpdateIndexBuffer(const std::vector<int>& indices) {
    if (indices.empty()) return true;

    // Resize buffer if needed
    if (indices.size() > _indexBufferCapacity) {
        _indexBufferCapacity = indices.size() * BUFFER_GROWTH_FACTOR;

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(int) * _indexBufferCapacity;
        ibDesc.Usage = D3D11_USAGE_DYNAMIC;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        _indexBuffer.Reset();
        HRESULT hr = _device->CreateBuffer(&ibDesc, nullptr, &_indexBuffer);
        if (FAILED(hr)) {
            std::cerr << "[RendererGPU] Failed to resize index buffer: 0x" << std::hex << hr << std::endl;
            return false;
        }
    }

    // Map and update
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(_indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to map index buffer: 0x" << std::hex << hr << std::endl;
        return false;
    }

    memcpy(mapped.pData, indices.data(), sizeof(int) * indices.size());
    _context->Unmap(_indexBuffer.Get(), 0);

    return true;
}

bool RendererGPU::UpdateConstantBuffer(const ConstantBuffer& cb) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to map constant buffer: 0x" << std::hex << hr << std::endl;
        return false;
    }

    memcpy(mapped.pData, &cb, sizeof(ConstantBuffer));
    _context->Unmap(_constantBuffer.Get(), 0);

    return true;
}

// =========================================================================
// Texture Management
// =========================================================================

bool RendererGPU::CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv) {
    if (!tex) return false;

    // Get texture dimensions and data
    int width = tex->GetWidth();
    int height = tex->GetHeight();
    const uint8_t* data = tex->GetDataPtr();

    if (!data) return false;  // Check if pointer is null

    // Convert from 0-15 range to 0-255 range for GPU
    const size_t pixelCount = width * height * 4;
    std::vector<uint8_t> gpuData(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i) {
        // Convert 0-15 to 0-255: value * 17 (since 15 * 17 = 255)
        gpuData[i] = data[i] * 17;
    }

    // Create texture with auto-generated mipmaps to reduce distant shimmering
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 0;  // Auto-generate all mip levels
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;  // Default usage for mipmap generation
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;  // RTV needed for GenerateMips
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;  // Enable mipmap generation

    // Create the texture without initial data (for mipmap generation)
    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = _device->CreateTexture2D(&texDesc, nullptr, &texture);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create texture: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Upload base mip level (level 0) manually
    _context->UpdateSubresource(texture.Get(), 0, nullptr, gpuData.data(), width * 4, 0);
    
    // Create shader resource view first
    hr = _device->CreateShaderResourceView(texture.Get(), nullptr, srv);
    if (FAILED(hr)) {
        std::cerr << "[RendererGPU] Failed to create SRV: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Auto-generate mipmaps from base level
    _context->GenerateMips(*srv);

    return true;
}

void RendererGPU::BindTexture(const Texture* tex) {
    if (!tex) {
        UnbindTexture();
        return;
    }

    // Check cache first
    auto it = _textureCache.find(tex);
    if (it != _textureCache.end()) {
        // Texture already uploaded - just bind it
        _currentTextureSRV = it->second;
        _context->PSSetShaderResources(0, 1, it->second.GetAddressOf());
        return;
    }

    // First time seeing this texture - create and cache it
    ID3D11ShaderResourceView* srv = nullptr;
    if (CreateTextureFromASCIIgLTexture(tex, &srv)) {
        _currentTextureSRV.Attach(srv);
        _textureCache[tex] = _currentTextureSRV;  // Cache for future use
        _context->PSSetShaderResources(0, 1, &srv);
    }
}

void RendererGPU::UnbindTexture() {
    _currentTextureSRV.Reset();
    ID3D11ShaderResourceView* nullSRV = nullptr;
    _context->PSSetShaderResources(0, 1, &nullSRV);
}

bool RendererGPU::InitializeDebugSwapChain() {
    // Get console window handle for minimal swap chain
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) {
        std::cerr << "[RendererGPU] Failed to get console window handle" << std::endl;
        return false;
    }

    // Create a minimal 1x1 swap chain for RenderDoc
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = 1;
    swapChainDesc.BufferDesc.Height = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Get DXGI factory from device
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(_device.As(&dxgiDevice))) {
        std::cerr << "[RendererGPU] Failed to get DXGI device" << std::endl;
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        std::cerr << "[RendererGPU] Failed to get DXGI adapter" << std::endl;
        return false;
    }

    ComPtr<IDXGIFactory> dxgiFactory;
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory))) {
        std::cerr << "[RendererGPU] Failed to get DXGI factory" << std::endl;
        return false;
    }

    if (FAILED(dxgiFactory->CreateSwapChain(_device.Get(), &swapChainDesc, &_debugSwapChain))) {
        std::cerr << "[RendererGPU] Failed to create debug swap chain" << std::endl;
        return false;
    }

    std::cout << "[RendererGPU] Debug swap chain created successfully" << std::endl;
    return true;
}

// =========================================================================
// Rendering Pipeline
// =========================================================================

void RendererGPU::BeginColBuffFrame() {
    if (!_initialized) {
        std::cerr << "[RendererGPU] BeginFrame called before initialization!" << std::endl;
        return;
    }

    Logger::Debug("BeginFrame: Clearing render target and setting up pipeline");

    // Update constant buffer with current values
    UpdateConstantBuffer(_currentConstantBuffer);

    // Clear render target
    float clear_color[4] = { _renderer->GetBackgroundCol().x, _renderer->GetBackgroundCol().y, 
                             _renderer->GetBackgroundCol().z, 1.0f };
    _context->ClearRenderTargetView(_renderTargetView.Get(), clear_color);

    // Clear depth stencil
    _context->ClearDepthStencilView(_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);  // Clear to 1.0 (far plane)

    // Bind render targets
    _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilView.Get());

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(Screen::GetInst().GetWidth());
    viewport.Height = static_cast<float>(Screen::GetInst().GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    _context->RSSetViewports(1, &viewport);

    // Set depth stencil state
    _context->OMSetDepthStencilState(_depthStencilState.Get(), 0);

    // Set rasterizer state based on wireframe, backface culling, and CCW modes
    bool wireframe = Renderer::GetInst().GetWireframe();
    bool backfaceCulling = Renderer::GetInst().GetBackfaceCulling();
    bool ccw = Renderer::GetInst().GetCCW();
    
    // Calculate index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    int stateIndex = (wireframe ? 1 : 0) + (backfaceCulling ? 2 : 0) + (ccw ? 4 : 0);
    _context->RSSetState(_rasterizerStates[stateIndex].Get());

    // Bind shaders
    _context->VSSetShader(_vertexShader.Get(), nullptr, 0);
    _context->PSSetShader(_pixelShader.Get(), nullptr, 0);
    _context->IASetInputLayout(_inputLayout.Get());

    // Update and bind constant buffer
    _context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
    _context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());

    // Bind sampler
    _context->PSSetSamplers(0, 1, _samplerLinear.GetAddressOf());

    // Set primitive topology
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RendererGPU::EndColBuffFrame() {
    // Resolve MSAA render target to non-MSAA texture for CPU download
    D3D11_TEXTURE2D_DESC rtDesc;
    _renderTarget->GetDesc(&rtDesc);
    
    if (rtDesc.SampleDesc.Count > 1) {
        // MSAA is enabled - resolve to non-MSAA texture
        _context->ResolveSubresource(
            _resolvedTexture.Get(),     // Destination (non-MSAA)
            0,                           // Dest subresource
            _renderTarget.Get(),         // Source (MSAA)
            0,                           // Source subresource
            DXGI_FORMAT_R8G8B8A8_UNORM  // Format
        );
    } else {
        // No MSAA - just copy render target to resolved texture
        _context->CopyResource(_resolvedTexture.Get(), _renderTarget.Get());
    }
    
    // Present for RenderDoc (does nothing if no swap chain)
    if (_debugSwapChain) {
        _debugSwapChain->Present(0, 0);
    }

    RendererGPU::GetInst().DownloadFramebuffer();
}

void RendererGPU::RenderTriangles(const std::vector<std::byte>& vertices, const VertFormat& format, const Texture* tex, const std::vector<int>& indices) {
    if (!_initialized || vertices.empty()) return;

    const UINT stride = format.GetStride();
    const size_t vertexCount = vertices.size() / stride;

    bool useIndices = !indices.empty();
    Logger::Debug("RenderTriangles: Drawing " + std::to_string(vertexCount) + " vertices" + 
                  (useIndices ? " with " + std::to_string(indices.size()) + " indices" : ""));

    // Update vertex buffer
    if (!UpdateVertexBuffer(vertices)) return;

    // Bind vertex buffer
    UINT offset = 0;
    _context->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);

    // Bind texture
    BindTexture(tex);

    if (useIndices) {
        // Indexed rendering
        if (!UpdateIndexBuffer(indices)) return;
        
        // Bind index buffer
        _context->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        
        // Draw indexed
        _context->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);
    } else {
        // Non-indexed rendering
        _context->Draw(static_cast<UINT>(vertexCount), 0);
    }
}

void RendererGPU::RenderTriangles(const std::vector<std::vector<std::byte>*>& vertices, const VertFormat& format, const Texture* tex, const std::vector<std::vector<int>>& indices) {
    const UINT stride = format.GetStride();
    
    // Flatten all vertex byte arrays and offset indices appropriately
    std::vector<std::byte> flatVertices;
    std::vector<int> flatIndices;
    
    int vertexOffset = 0;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto* vertArray = vertices[i];
        if (vertArray && !vertArray->empty()) {
            const size_t vertexCount = vertArray->size() / stride;
            
            // Add vertices (byte data)
            flatVertices.insert(flatVertices.end(), vertArray->begin(), vertArray->end());
            
            // Add indices with offset
            if (i < indices.size()) {
                const auto& indexArray = indices[i];
                for (int index : indexArray) {
                    flatIndices.push_back(index + vertexOffset);
                }
            }
            
            // Update offset for next mesh (in vertex count, not bytes)
            vertexOffset += static_cast<int>(vertexCount);
        }
    }

    RenderTriangles(flatVertices, format, tex, flatIndices);
}

// =========================================================================
// Framebuffer Download (GPU -> CPU)
// =========================================================================

void RendererGPU::DownloadFramebuffer()
{
    Logger::Debug("[RendererGPU] Downloading framebuffer from GPU to CPU...");

    if (!_initialized) {
        Logger::Warning("[RendererGPU] DownloadFramebuffer called before initialization!");
        return;
    };

    Logger::Debug("DownloadFramebuffer: Copying GPU framebuffer to CPU");

    const int width = Screen::GetInst().GetWidth();
    const int height = Screen::GetInst().GetHeight();

    // Ensure color buffer is correct size
    if (_renderer->_color_buffer.size() != width * height) {
        _renderer->_color_buffer.resize(width * height);
    }

    // Copy resolved texture to staging texture (resolved texture is always non-MSAA)
    _context->CopyResource(_stagingTexture.Get(), _resolvedTexture.Get());

    // Map staging texture
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        Logger::Warning("Failed to map staging texture: 0x" + std::to_string(hr));
        return;
    }

    // Copy pixels to color buffer (convert 0-255 to 0-15 for terminal palette)
    uint8_t* src = static_cast<uint8_t*>(mapped.pData);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = src + y * mapped.RowPitch + x * 4; // RGBA
            int index = y * width + x;
            
            // Convert 0-255 to 0-15 (4-bit color)
            _renderer->_color_buffer[index] = glm::ivec4(
                pixel[0] / 17,  // R
                pixel[1] / 17,  // G
                pixel[2] / 17,  // B
                pixel[3] / 17   // A
            );
        }
    }

    _context->Unmap(_stagingTexture.Get(), 0);
    
    // Sample some pixels to verify we have actual data
    int nonBlackPixels = 0;
    for (int i = 0; i < width * height; i++) {
        auto& pixel = _renderer->_color_buffer[i];
        if (pixel.r != 0 || pixel.g != 0 || pixel.b != 0) {
            nonBlackPixels++;
        }
    }
    Logger::Debug("DownloadFramebuffer: Successfully downloaded " + std::to_string(width * height) + " pixels, " + std::to_string(nonBlackPixels) + " non-black");
    
    // Sample center pixel
    int centerIdx = (height / 2) * width + (width / 2);
    auto& centerPixel = _renderer->_color_buffer[centerIdx];
    Logger::Debug("Center pixel color: R=" + std::to_string(centerPixel.r) + " G=" + std::to_string(centerPixel.g) + " B=" + std::to_string(centerPixel.b));
}

// =========================================================================
// Shader Uniforms
// =========================================================================

void RendererGPU::SetMVP(const glm::mat4& mvp) {
    _currentConstantBuffer.mvp = mvp;
    
    // Log MVP matrix (first time or when it changes significantly)
    static bool firstTime = true;
    if (firstTime) {
        Logger::Debug("MVP Matrix set - [0][0]=" + std::to_string(mvp[0][0]) + " [3][3]=" + std::to_string(mvp[3][3]));
        firstTime = false;
    }
}

// =========================================================================
// Cleanup
// =========================================================================

void RendererGPU::Shutdown()
{
    if (!_initialized) return;

    // Release all COM objects (ComPtr handles this automatically)
    _textureCache.clear();  // Clear texture cache
    _currentTextureSRV.Reset();
    _stagingTexture.Reset();
    
    // Release sampler
    _samplerLinear.Reset();
    
    // Release all rasterizer states
    for (auto& state : _rasterizerStates) {
        state.Reset();
    }
    
    _vertexBuffer.Reset();
    _indexBuffer.Reset();
    _constantBuffer.Reset();
    _inputLayout.Reset();
    _pixelShader.Reset();
    _vertexShader.Reset();
    _depthStencilState.Reset();
    _depthStencilView.Reset();
    _depthStencilBuffer.Reset();
    _renderTargetView.Reset();
    _resolvedTexture.Reset();
    _renderTarget.Reset();
    _context.Reset();
    _device.Reset();

    _initialized = false;
    std::cout << "[RendererGPU] Shutdown complete" << std::endl;
}

// =========================================================================
// Per-Mesh GPU Buffer Cache Management
// =========================================================================

RendererGPU::GPUMeshCache* RendererGPU::GetOrCreateMeshCache(const Mesh* mesh) {
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
            std::cerr << "[RendererGPU] Failed to create mesh vertex buffer: 0x" << std::hex << hr << std::endl;
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
            std::cerr << "[RendererGPU] Failed to create mesh index buffer: 0x" << std::hex << hr << std::endl;
            delete cache;
            return nullptr;
        }
        cache->indexCount = mesh->GetIndices().size();
    }
    
    // Store cache in mesh
    mesh->gpuBufferCache = cache;
    
    Logger::Debug("[RendererGPU] Created GPU buffer cache for mesh: " + 
                  std::to_string(cache->vertexCount) + " vertices, " + 
                  std::to_string(cache->indexCount) + " indices");
    
    return cache;
}

void RendererGPU::ReleaseMeshCache(void* cachePtr) {
    if (!cachePtr) return;
    
    GPUMeshCache* cache = static_cast<GPUMeshCache*>(cachePtr);
    delete cache;  // ComPtr automatically releases buffers
}

void RendererGPU::InvalidateTextureCache(const Texture* tex) {
    if (!tex) return;
    
    auto it = _textureCache.find(tex);
    if (it != _textureCache.end()) {
        _textureCache.erase(it);
        Logger::Debug("[RendererGPU] Texture cache invalidated for texture");
    }
}

// =============================================================================
// PUBLIC HIGH-LEVEL DRAWING FUNCTIONS
// =============================================================================

void RendererGPU::DrawMesh(const Mesh* mesh) {
    if (!_initialized || !mesh) return;
    
    // Get or create GPU buffer cache for this mesh
    GPUMeshCache* cache = GetOrCreateMeshCache(mesh);
    if (!cache || !cache->vertexBuffer) return;
    
    // Bind cached vertex buffer with mesh's format stride
    UINT stride = mesh->GetVertFormat().GetStride();
    UINT offset = 0;
    _context->IASetVertexBuffers(0, 1, cache->vertexBuffer.GetAddressOf(), &stride, &offset);
    
    // Bind texture
    BindTexture(mesh->GetTexture());
    
    // Draw with or without indices
    if (cache->indexBuffer && cache->indexCount > 0) {
        _context->IASetIndexBuffer(cache->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        _context->DrawIndexed(static_cast<UINT>(cache->indexCount), 0, 0);
    } else {
        _context->Draw(static_cast<UINT>(cache->vertexCount), 0);
    }
}

void RendererGPU::DrawMesh(const Mesh* mesh, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera) {
    if (!mesh) return;
    
    glm::mat4 model = MathUtil::CalcModelMatrix(position, rotation, size);
    glm::mat4 mvp = camera.proj * camera.view * model;
 
    // Set MVP matrix for GPU rendering
    SetMVP(mvp);
    
    // Create byte vector from mesh vertex data
    std::vector<std::byte> vertexBytes(mesh->GetVertices().data(), mesh->GetVertices().data() + mesh->GetVertexDataSize());
    RenderTriangles(vertexBytes, mesh->GetVertFormat(), mesh->texture, mesh->GetIndices());
}

void RendererGPU::DrawModel(const Model& ModelObj, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& size, const Camera3D& camera) {
    glm::mat4 model = MathUtil::CalcModelMatrix(position, rotation, size);
    glm::mat4 mvp = camera.proj * camera.view * model;

    // Set MVP matrix for GPU rendering
    SetMVP(mvp);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(ModelObj.meshes[i]);
    }
}

void RendererGPU::DrawModel(const Model& ModelObj, const glm::mat4& model, const Camera3D& camera)
{
    glm::mat4 mvp = camera.proj * camera.view * model;

    // Set MVP matrix for GPU rendering
    SetMVP(mvp);

    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(ModelObj.meshes[i]);
    }
}

void RendererGPU::Draw2DQuadPixelSpace(const Texture& tex, const glm::vec2& position, const float rotation, const glm::vec2& size, const Camera2D& camera, const int layer, const bool CCW) {
    glm::mat4 model = MathUtil::CalcModelMatrix(glm::vec3(position, layer), rotation, glm::vec3(size, 0.0f));
    glm::mat4 mvp = camera.proj * camera.view * model;

    // Set MVP matrix for GPU rendering
    SetMVP(mvp);

    // Quad vertices as PosUVW format (XYZW + UVW = 7 floats = 28 bytes per vertex)
    static const float CCWquadVerts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // 0: bottom-left
        -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // 1: top-left
         1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 2: top-right
         1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f   // 3: bottom-right
    };

    static const float CWquadVerts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // 0: bottom-left
        -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // 1: top-left
         1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 2: top-right
         1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f   // 3: bottom-right
    };

    static const std::vector<int> CCWindices = {
        0, 1, 2,  // Triangle 1: bottom-left → top-left → top-right
        0, 2, 3   // Triangle 2: bottom-left → top-right → bottom-right
    };

    static const std::vector<int> CWindices = {
        0, 2, 1,  // Triangle 1: bottom-left → top-right → top-left
        0, 3, 2   // Triangle 2: bottom-left → bottom-right → top-right
    };

    // Convert to byte vector
    const float* verts = CCW ? CCWquadVerts : CWquadVerts;
    const auto* vertBytes = reinterpret_cast<const std::byte*>(verts);
    std::vector<std::byte> quadBytes(vertBytes, vertBytes + sizeof(CCWquadVerts));

    RenderTriangles(quadBytes, VertFormats::PosUVW(), &tex, CCW ? CCWindices : CWindices);
}

void RendererGPU::Draw2DQuadPercSpace(const Texture& tex, const glm::vec2& positionPerc, const float rotation, const glm::vec2& sizePerc, const Camera2D& camera, const int layer, const bool CCW)
{
    // Convert percentage coordinates to pixel coordinates
    float screenWidth = static_cast<float>(Screen::GetInst().GetWidth());
    float screenHeight = static_cast<float>(Screen::GetInst().GetHeight());
    
    glm::vec2 pixelPosition = glm::vec2(positionPerc.x * screenWidth, positionPerc.y * screenHeight);
    glm::vec2 pixelSize = glm::vec2(sizePerc.x * screenWidth, sizePerc.y * screenHeight);
    
    glm::mat4 model = MathUtil::CalcModelMatrix(glm::vec3(pixelPosition, layer), rotation, glm::vec3(pixelSize, 0.0f));
    glm::mat4 mvp = camera.proj * camera.view * model;

    // Set MVP matrix for GPU rendering
    SetMVP(mvp);

    // Quad vertices as PosUVW format (XYZW + UVW = 7 floats = 28 bytes per vertex)
    static const float CCWquadVerts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // 0: bottom-left
        -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // 1: top-left
         1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 2: top-right
         1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f   // 3: bottom-right
    };

    static const float CWquadVerts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // 0: bottom-left
        -1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // 1: top-left
         1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // 2: top-right
         1.0f, -1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f   // 3: bottom-right
    };

    static const std::vector<int> CCWindices = {
        0, 1, 2,  // Triangle 1: bottom-left → top-left → top-right
        0, 2, 3   // Triangle 2: bottom-left → top-right → bottom-right
    };

    static const std::vector<int> CWindices = {
        0, 2, 1,  // Triangle 1: bottom-left → top-right → top-left
        0, 3, 2   // Triangle 2: bottom-left → bottom-right → top-right
    };

    // Convert to byte vector
    const float* verts = CCW ? CCWquadVerts : CWquadVerts;
    const auto* vertBytes = reinterpret_cast<const std::byte*>(verts);
    std::vector<std::byte> quadBytes(vertBytes, vertBytes + sizeof(CCWquadVerts));

    RenderTriangles(quadBytes, VertFormats::PosUVW(), &tex, CCW ? CCWindices : CWindices);
}
} // namespace ASCIIgL
