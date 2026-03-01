#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>              // std::clamp
#include <sstream>                // std::ostringstream

#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

// =============================================================================
// LIFECYCLE MANAGEMENT
// =============================================================================

Renderer::~Renderer() {
    Shutdown();
    _color_buffer.clear();
}

void Renderer::Initialize(bool antialiasing, int antialiasing_samples) {
    _antialiasing = antialiasing;
    _antialiasing_samples = antialiasing_samples;
    
    if (!_initialized) {
        Logger::Info("Initializing Renderer...");
    } else {
        Logger::Warning("Renderer is already initialized!");
        return;
    }

    if (!Screen::GetInst().IsInitialized()) {
        Logger::Error("Renderer: Screen must be initialized before creating Renderer.");
        throw std::runtime_error("Renderer: Screen must be initialized before creating Renderer.");
    }
    auto& screen = Screen::GetInst();
    _color_buffer.resize(screen.GetWidth() * screen.GetHeight());

    LoadCharCoverageFromJson();

    Logger::Info("[Renderer] Initializing DirectX 11...");

    if (!InitializeDevice()) {
        Logger::Error("[Renderer] Failed to initialize device");
        return;
    }

    if (!InitializeRenderTarget()) {
        Logger::Error("[Renderer] Failed to initialize render target");
        return;
    }

    Logger::Info("[Renderer] Initializing depth stencil...");
    if (!InitializeDepthStencil()) {
        Logger::Error("[Renderer] Failed to initialize depth stencil");
        return;
    }

    if (!InitializeSamplers()) {
        Logger::Error("[Renderer] Failed to initialize samplers");
        return;
    }

    if (!InitializeRasterizerStates()) {
        Logger::Error("[Renderer] Failed to initialize rasterizer states");
        return;
    }

    Logger::Info("[Renderer] Initializing blend states...");
    if (!InitializeBlendStates()) {
        Logger::Error("[Renderer] Failed to initialize blend states");
        return;
    }

    if (!InitializeStagingTexture()) {
        Logger::Error("[Renderer] Failed to initialize staging texture");
        return;
    }

    if (!InitializeDebugSwapChain()) {
        Logger::Error("[Renderer] Failed to initialize debug swap chain (non-fatal)");
        // Non-fatal - continue without swap chain
    }

    // Pipeline state (depth + blend) is set in BeginColBuffFrame when RTV is bound.

    _initialized = true;
    Logger::Debug("Renderer initialization complete - Device, Shaders, Buffers ready");
}

bool Renderer::IsInitialized() const {
    return _initialized;
}

// =========================================================================
// GPU Initialization Methods
// =========================================================================

bool Renderer::InitializeDevice() {
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
        Logger::Error("[Renderer] D3D11CreateDevice failed: 0x" + ss.str());
        return false;
    }

    std::string featureLevelStr = (featureLevel == D3D_FEATURE_LEVEL_11_1) ? "11.1" : "11.0";
    Logger::Info("[Renderer] Device created with feature level: " + featureLevelStr);

    return true;
}

bool Renderer::InitializeRenderTarget() {
    // Get MSAA settings
    bool useMSAA = GetAntialiasing();
    int msaaSamples = GetAntialiasingsamples();
    
    UINT sampleCount = 1;
    UINT qualityLevel = 0;
    
    if (useMSAA) {
        sampleCount = std::clamp(msaaSamples, 1, 8);  // Clamp to 1-8 samples
        
        // Check MSAA quality support
        UINT numQualityLevels = 0;
        _device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, sampleCount, &numQualityLevels);
        if (numQualityLevels > 0) {
            qualityLevel = 0; 
        } else {
            Logger::Warning("[Renderer] " + std::to_string(sampleCount) + " x MSAA not supported, falling back to 1x");
            sampleCount = 1;
        }
    }
    
    Logger::Debug("[Renderer] Creating render target with " + std::to_string(sampleCount) + "x MSAA");
    
    // Create MSAA render target texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = Screen::GetInst().GetWidth();
    texDesc.Height = Screen::GetInst().GetHeight();
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    texDesc.SampleDesc.Count = sampleCount;
    texDesc.SampleDesc.Quality = qualityLevel;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;

    HRESULT hr = _device->CreateTexture2D(&texDesc, nullptr, &_renderTarget);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create render target texture: 0x" + ss.str());
        return false;
    }

    // Create render target view
    hr = _device->CreateRenderTargetView(_renderTarget.Get(), nullptr, &_renderTargetView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create render target view: 0x" + ss.str());
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
        Logger::Error("[Renderer] Failed to create resolved texture: 0x" + ss.str());
        return false;
    }

    std::string msg = "[Renderer] Render target initialized: " + 
        std::to_string(Screen::GetInst().GetWidth()) + "x" + 
        std::to_string(Screen::GetInst().GetHeight()) + 
        (useMSAA ? (" with " + std::to_string(sampleCount) + "x MSAA") : " without MSAA");
    Logger::Info(msg);

    return true;
}

bool Renderer::InitializeDepthStencil() {
    // Get MSAA settings (must match render target)
    bool useMSAA = GetAntialiasing();
    int msaaSamples = GetAntialiasingsamples();
    
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
        Logger::Error("[Renderer] Failed to create depth stencil buffer: 0x" + ss.str());
        return false;
    }

    // Create depth stencil view
    hr = _device->CreateDepthStencilView(_depthStencilBuffer.Get(), nullptr, &_depthStencilView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create depth stencil view: 0x" + ss.str());
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
        Logger::Error("[Renderer] Failed to create depth stencil state: 0x" + ss.str());
        return false;
    }

    // No-depth state (depth test disabled)
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = _device->CreateDepthStencilState(&dsDesc, &_depthStencilStateNoTest);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create no-depth stencil state: 0x" + ss.str());
        return false;
    }

    // Depth test on, depth write off — for transparent/2D so they don't occlude geometry behind
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = _device->CreateDepthStencilState(&dsDesc, &_depthStencilStateNoWrite);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create no-write depth stencil state: 0x" + ss.str());
        return false;
    }

    return true;
}

bool Renderer::InitializeSamplers() {
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
        Logger::Error("[Renderer] Failed to create sampler state: 0x" + ss.str());
        return false;
    }

    Logger::Debug("[Renderer] Created point sampler with linear mipmap blending for pixel art");
    return true;
}

bool Renderer::InitializeRasterizerStates() {
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
            Logger::Error("[Renderer] Failed to create rasterizer state " + std::to_string(i) + ": 0x" + ss.str());
            return false;
        }
    }

    return true;
}

bool Renderer::InitializeBlendStates() {
    // Opaque: no blending (default for 3D)
    D3D11_BLEND_DESC opaqueDesc = {};
    opaqueDesc.AlphaToCoverageEnable = FALSE;
    opaqueDesc.IndependentBlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].BlendEnable = FALSE;
    opaqueDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = _device->CreateBlendState(&opaqueDesc, &_blendStateOpaque);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create opaque blend state: 0x" + ss.str());
        return false;
    }

    // Alpha: blend for GUI (transparent PNG regions)
    D3D11_BLEND_DESC alphaDesc = {};
    alphaDesc.AlphaToCoverageEnable = FALSE;
    alphaDesc.IndependentBlendEnable = FALSE;
    alphaDesc.RenderTarget[0].BlendEnable = TRUE;
    alphaDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alphaDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alphaDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    alphaDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    alphaDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    alphaDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alphaDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = _device->CreateBlendState(&alphaDesc, &_blendStateAlpha);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create alpha blend state: 0x" + ss.str());
        return false;
    }

    return true;
}

bool Renderer::InitializeStagingTexture() {
    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = Screen::GetInst().GetWidth();
    stagingDesc.Height = Screen::GetInst().GetHeight();
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = _device->CreateTexture2D(&stagingDesc, nullptr, &_stagingTexture);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create staging texture: 0x" + ss.str());
        return false;
    }

    return true;
}

bool Renderer::InitializeDebugSwapChain() {
    // Get console window handle for minimal swap chain
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) {
        Logger::Error("[Renderer] Failed to get console window handle");
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
        Logger::Error("[Renderer] Failed to get DXGI device");
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        Logger::Error("[Renderer] Failed to get DXGI adapter");
        return false;
    }

    ComPtr<IDXGIFactory> dxgiFactory;
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory))) {
        Logger::Error("[Renderer] Failed to get DXGI factory");
        return false;
    }

    if (FAILED(dxgiFactory->CreateSwapChain(_device.Get(), &swapChainDesc, &_debugSwapChain))) {
        Logger::Error("[Renderer] Failed to create debug swap chain");
        return false;
    }

    Logger::Debug("[Renderer] Debug swap chain created successfully");
    return true;
}

// =========================================================================
// Framebuffer Download (GPU -> CPU)
// =========================================================================

void Renderer::DownloadFramebuffer()
{
    Logger::Debug("[Renderer] Downloading framebuffer from GPU to CPU...");

    if (!_initialized) {
        Logger::Warning("[Renderer] DownloadFramebuffer called before initialization!");
        return;
    };

    Logger::Debug("DownloadFramebuffer: Copying GPU framebuffer to CPU");

    const int width = Screen::GetInst().GetWidth();
    const int height = Screen::GetInst().GetHeight();

    // Ensure color buffer is correct size
    if (_color_buffer.size() != width * height) {
        _color_buffer.resize(width * height);
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

    // Copy pixels to color buffer (keep 0-255, convert to 0-15 at LUT lookup)
    uint8_t* src = static_cast<uint8_t*>(mapped.pData);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = src + y * mapped.RowPitch + x * 4; // RGBA
            int index = y * width + x;
            
            // Keep 0-255 precision in color buffer
            _color_buffer[index] = glm::ivec4(
                pixel[0],  // R (0-255)
                pixel[1],  // G (0-255)
                pixel[2],  // B (0-255)
                pixel[3]   // A (0-255)
            );
        }
    }

    _context->Unmap(_stagingTexture.Get(), 0);
    
    // Sample some pixels to verify we have actual data
    int nonBlackPixels = 0;
    for (int i = 0; i < width * height; i++) {
        auto& pixel = _color_buffer[i];
        if (pixel.r != 0 || pixel.g != 0 || pixel.b != 0) {
            nonBlackPixels++;
        }
    }
    Logger::Debug("DownloadFramebuffer: Successfully downloaded " + std::to_string(width * height) + " pixels, " + std::to_string(nonBlackPixels) + " non-black");
    
    // Sample center pixel
    int centerIdx = (height / 2) * width + (width / 2);
    auto& centerPixel = _color_buffer[centerIdx];
    Logger::Debug("Center pixel color: R=" + std::to_string(centerPixel.r) + " G=" + std::to_string(centerPixel.g) + " B=" + std::to_string(centerPixel.b));
}

// =========================================================================
// Cleanup
// =========================================================================

void Renderer::Shutdown()
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

    _blendStateAlpha.Reset();
    _blendStateOpaque.Reset();
    _depthStencilStateNoWrite.Reset();
    _depthStencilStateNoTest.Reset();
    _depthStencilState.Reset();
    _depthStencilView.Reset();
    _depthStencilBuffer.Reset();
    _renderTargetView.Reset();
    _resolvedTexture.Reset();
    _renderTarget.Reset();
    _context.Reset();
    _device.Reset();

    _initialized = false;
    Logger::Debug("[Renderer] Shutdown complete");
}

} // namespace ASCIIgL

