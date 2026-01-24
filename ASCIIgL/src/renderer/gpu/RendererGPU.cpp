#include <ASCIIgL/renderer/gpu/RendererGPU.hpp>

#include <iostream>
#include <sstream>
#include <Windows.h>
#include <algorithm>

#include <ASCIIgL/renderer/Renderer.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/renderer/gpu/Shader.hpp>
#include <ASCIIgL/renderer/gpu/Material.hpp>

#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/MathUtil.hpp>

namespace ASCIIgL {

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

    Logger::Info("[RendererGPU] Initializing DirectX 11...");

    if (!InitializeDevice()) {
        Logger::Error("[RendererGPU] Failed to initialize device");
        return;
    }

    if (!InitializeRenderTarget()) {
        Logger::Error("[RendererGPU] Failed to initialize render target");
        return;
    }

    if (!InitializeDepthStencil()) {
        Logger::Error("[RendererGPU] Failed to initialize depth stencil");
        return;
    }

    if (!InitializeSamplers()) {
        Logger::Error("[RendererGPU] Failed to initialize samplers");
        return;
    }

    if (!InitializeRasterizerStates()) {
        Logger::Error("[RendererGPU] Failed to initialize rasterizer states");
        return;
    }

    if (!InitializeStagingTexture()) {
        Logger::Error("[RendererGPU] Failed to initialize staging texture");
        return;
    }

    if (!InitializeDebugSwapChain()) {
        Logger::Error("[RendererGPU] Failed to initialize debug swap chain (non-fatal)");
        // Non-fatal - continue without swap chain
    }

    // Initialize static quad meshes for 2D rendering
    InitializeQuadMeshes();

    _initialized = true;
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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] D3D11CreateDevice failed: 0x" + ss.str());
        return false;
    }

    std::string featureLevelStr = (featureLevel == D3D_FEATURE_LEVEL_11_1) ? "11.1" : "11.0";
    Logger::Info("[RendererGPU] Device created with feature level: " + featureLevelStr);

    return true;
}

bool RendererGPU::InitializeRenderTarget() {
    // Get MSAA settings
    bool useMSAA = Renderer::GetInst().GetAntialiasing();
    int msaaSamples = Renderer::GetInst().GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    UINT qualityLevel = 0;
    
    if (useMSAA) {
        sampleCount = std::clamp(msaaSamples, 1, 8);  // Clamp to 1-8 samples
        
        // Check MSAA quality support
        UINT numQualityLevels = 0;
        _device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, sampleCount, &numQualityLevels);
        if (numQualityLevels > 0) {
            qualityLevel = 0;  // Use highest quality
        } else {
            Logger::Warning("[RendererGPU] " + std::to_string(sampleCount) + " x MSAA not supported, falling back to 1x");
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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create render target texture: 0x" + ss.str());
        return false;
    }

    // Create render target view
    hr = _device->CreateRenderTargetView(_renderTarget.Get(), nullptr, &_renderTargetView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create render target view: 0x" + ss.str());
        return false;
    }
    
    // Create resolved (non-MSAA) texture for CPU download
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.BindFlags = 0;  // No binding needed, just for copying
    
    hr = _device->CreateTexture2D(&texDesc, nullptr, &_resolvedTexture);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create resolved texture: 0x" + ss.str());
        return false;
    }

    std::string msg = "[RendererGPU] Render target initialized: " + 
        std::to_string(Screen::GetInst().GetWidth()) + "x" + 
        std::to_string(Screen::GetInst().GetHeight()) + 
        (useMSAA ? (" with " + std::to_string(sampleCount) + "x MSAA") : " without MSAA");
    Logger::Info(msg);

    return true;
}

bool RendererGPU::InitializeDepthStencil() {
    // Get MSAA settings (must match render target)
    bool useMSAA = Renderer::GetInst().GetAntialiasing();
    int msaaSamples = Renderer::GetInst().GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    if (useMSAA) {
        sampleCount = std::clamp(msaaSamples, 1, 8);
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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create depth stencil buffer: 0x" + ss.str());
        return false;
    }

    // Create depth stencil view
    hr = _device->CreateDepthStencilView(_depthStencilBuffer.Get(), nullptr, &_depthStencilView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create depth stencil view: 0x" + ss.str());
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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create depth stencil state: 0x" + ss.str());
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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create sampler state: 0x" + ss.str());
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
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[RendererGPU] Failed to create rasterizer state " + std::to_string(i) + ": 0x" + ss.str());
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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create staging texture: 0x" + ss.str());
        return false;
    }

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
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create texture: 0x" + ss.str());
        return false;
    }

    // Upload base mip level (level 0) manually
    _context->UpdateSubresource(texture.Get(), 0, nullptr, gpuData.data(), width * 4, 0);
    
    // Create shader resource view first
    hr = _device->CreateShaderResourceView(texture.Get(), nullptr, srv);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[RendererGPU] Failed to create SRV: 0x" + ss.str());
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
        Logger::Error("[RendererGPU] Failed to get console window handle");
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
        Logger::Error("[RendererGPU] Failed to get DXGI device");
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        Logger::Error("[RendererGPU] Failed to get DXGI adapter");
        return false;
    }

    ComPtr<IDXGIFactory> dxgiFactory;
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory))) {
        Logger::Error("[RendererGPU] Failed to get DXGI factory");
        return false;
    }

    if (FAILED(dxgiFactory->CreateSwapChain(_device.Get(), &swapChainDesc, &_debugSwapChain))) {
        Logger::Error("[RendererGPU] Failed to create debug swap chain");
        return false;
    }

    Logger::Debug("[RendererGPU] Debug swap chain created successfully");
    return true;
}

void RendererGPU::InitializeQuadMeshes() {
    // Quad vertices as PosUV format (XYZ + UV = 5 floats = 20 bytes per vertex)
    // Counter-clockwise winding
    const VertStructs::PosUV CCWquadVerts[] = {
        {{-1.0f, -1.0f, 0.0f, 0.0f, 0.0f}},  // 0: bottom-left
        {{-1.0f,  1.0f, 0.0f, 0.0f, 1.0f}},  // 1: top-left
        {{ 1.0f,  1.0f, 0.0f, 1.0f, 1.0f}},  // 2: top-right
        {{ 1.0f, -1.0f, 0.0f, 1.0f, 0.0f}}   // 3: bottom-right
    };

    // Clockwise winding (same vertices, different index order)
    const VertStructs::PosUV CWquadVerts[] = {
        {{-1.0f, -1.0f, 0.0f, 0.0f, 0.0f}},  // 0: bottom-left
        {{-1.0f,  1.0f, 0.0f, 0.0f, 1.0f}},  // 1: top-left
        {{ 1.0f,  1.0f, 0.0f, 1.0f, 1.0f}},  // 2: top-right
        {{ 1.0f, -1.0f, 0.0f, 1.0f, 0.0f}}   // 3: bottom-right
    };

    const std::vector<int> CCWindices = {
        0, 1, 2,  // Triangle 1: bottom-left → top-left → top-right
        0, 2, 3   // Triangle 2: bottom-left → top-right → bottom-right
    };

    const std::vector<int> CWindices = {
        0, 2, 1,  // Triangle 1: bottom-left → top-right → top-left
        0, 3, 2   // Triangle 2: bottom-left → bottom-right → top-right
    };

    // Convert to byte vectors
    const auto* ccwBytes = reinterpret_cast<const std::byte*>(CCWquadVerts);
    const auto* cwBytes = reinterpret_cast<const std::byte*>(CWquadVerts);
    
    std::vector<std::byte> ccwVertexData(ccwBytes, ccwBytes + sizeof(CCWquadVerts));
    std::vector<std::byte> cwVertexData(cwBytes, cwBytes + sizeof(CWquadVerts));

    // Create meshes (without texture - texture will be set per draw call)
    std::vector<int> ccwIndicesCopy = CCWindices;
    std::vector<int> cwIndicesCopy = CWindices;
    
    _quadMeshCCW = new Mesh(std::move(ccwVertexData), VertFormats::PosUV(), std::move(ccwIndicesCopy), nullptr);
    _quadMeshCW = new Mesh(std::move(cwVertexData), VertFormats::PosUV(), std::move(cwIndicesCopy), nullptr);

    Logger::Debug("[RendererGPU] Static quad meshes created (CCW and CW)");
}

void RendererGPU::CleanupQuadMeshes() {
    if (_quadMeshCCW) {
        delete _quadMeshCCW;
        _quadMeshCCW = nullptr;
    }
    if (_quadMeshCW) {
        delete _quadMeshCW;
        _quadMeshCW = nullptr;
    }
}

// =========================================================================
// Rendering Pipeline
// =========================================================================

void RendererGPU::BeginColBuffFrame() {
    if (!_initialized) {
        Logger::Error("[RendererGPU] BeginFrame called before initialization!");
        return;
    }

    Logger::Debug("BeginFrame: Clearing render target and setting up pipeline");

    // Clear render target (convert 0-15 integer color to 0-1 float)
    glm::ivec3 bgCol = _renderer->GetBackgroundCol();
    float clear_color[4] = { 
        static_cast<float>(bgCol.x) / 15.0f, 
        static_cast<float>(bgCol.y) / 15.0f, 
        static_cast<float>(bgCol.z) / 15.0f, 
        1.0f 
    };
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
// Cleanup
// =========================================================================

void RendererGPU::Shutdown()
{
    if (!_initialized) return;

    // Clean up static quad meshes
    CleanupQuadMeshes();

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

    _depthStencilState.Reset();
    _depthStencilView.Reset();
    _depthStencilBuffer.Reset();
    _renderTargetView.Reset();
    _resolvedTexture.Reset();
    _renderTarget.Reset();
    _context.Reset();
    _device.Reset();

    _initialized = false;
    Logger::Debug("[RendererGPU] Shutdown complete");
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
            std::ostringstream ss;
            ss << std::hex << hr;
            Logger::Error("[RendererGPU] Failed to create mesh vertex buffer: 0x" + ss.str());
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
            Logger::Error("[RendererGPU] Failed to create mesh index buffer: 0x" + ss.str());
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
    if (!mesh->GetTexture()) { return; }
    BindTexture(mesh->GetTexture());
    
    // Draw with or without indices
    if (cache->indexBuffer && cache->indexCount > 0) {
        _context->IASetIndexBuffer(cache->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        _context->DrawIndexed(static_cast<UINT>(cache->indexCount), 0, 0);
    } else {
        _context->Draw(static_cast<UINT>(cache->vertexCount), 0);
    }
}

void RendererGPU::DrawModel(const Model& ModelObj) {
    for (size_t i = 0; i < ModelObj.meshes.size(); i++) {
        DrawMesh(ModelObj.meshes[i]);
    }
}

void RendererGPU::Draw2DQuad(const Texture& tex) {
    if (!_initialized || (!_quadMeshCCW && !_quadMeshCW)) return;

    // Select appropriate quad mesh
    Mesh* quadMesh = _renderer->_ccw ? _quadMeshCCW : _quadMeshCW;
    
    // Temporarily set texture on mesh
    Texture* originalTexture = quadMesh->texture;
    quadMesh->texture = const_cast<Texture*>(&tex);
    
    // Draw using cached mesh
    DrawMesh(quadMesh);
    
    // Restore original texture (nullptr)
    quadMesh->texture = originalTexture;
}

// =========================================================================
// Custom Shader/Material System Implementation
// =========================================================================

void RendererGPU::BindShaderProgram(ShaderProgram* program) {
    if (!_initialized) return;
    
    _boundShaderProgram = program;
    
    if (program && program->IsValid()) {
        // Bind custom shaders and input layout
        _context->VSSetShader(program->_vertexShader->_vertexShader.Get(), nullptr, 0);
        _context->PSSetShader(program->_pixelShader->_pixelShader.Get(), nullptr, 0);
        _context->IASetInputLayout(program->_inputLayout.Get());
    } else {
        // Revert to default shaders
        UnbindShaderProgram();
    }
}

void RendererGPU::UnbindShaderProgram() {
    if (!_initialized) return;
    
    _boundShaderProgram = nullptr;
    _boundMaterial = nullptr;
}

void RendererGPU::BindMaterial(Material* material) {
    if (!_initialized || !material) return;
    
    _boundMaterial = material;
    
    // Bind shader program
    auto program = material->GetShaderProgram();
    if (program) {
        BindShaderProgram(program.get());
    }
    
    // Bind textures
    for (const auto& slot : material->_textureSlots) {
        if (slot.texture) {
            BindTexture(slot.texture);
            // If we had multiple texture slots, we'd set them to different registers
            // For now, we only support slot 0 (diffuse)
        }
    }
    
    // Upload constant buffer if dirty
    UploadMaterialConstants(material);
}

void RendererGPU::UploadMaterialConstants(Material* material) {
    if (!material || !_initialized) return;
    
    // Update material's packed constant buffer data
    material->UpdateConstantBufferData();
    
    auto program = material->GetShaderProgram();
    if (!program) return;
    
    const auto& layout = program->GetUniformLayout();
    if (layout.GetSize() == 0) return;
    
    // Create or update material's GPU constant buffer
    if (!material->_constantBufferInitialized) {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = layout.GetSize();
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        HRESULT hr = _device->CreateBuffer(&cbDesc, nullptr, &material->_constantBuffer);
        if (FAILED(hr)) {
            Logger::Error("Failed to create material constant buffer");
            return;
        }
        material->_constantBufferInitialized = true;
    }
    
    // Map and upload data
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(material->_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, material->_constantBufferData.data(), layout.GetSize());
        _context->Unmap(material->_constantBuffer.Get(), 0);
    }
    
    // Bind to both vertex and pixel shader stages
    _context->VSSetConstantBuffers(0, 1, material->_constantBuffer.GetAddressOf());
    _context->PSSetConstantBuffers(0, 1, material->_constantBuffer.GetAddressOf());
}

} // namespace ASCIIgL
