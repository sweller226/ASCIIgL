#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>              // std::clamp
#include <cstring>                // strlen
#include <sstream>                // std::ostringstream

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <ASCIIgL/renderer/HLSLIncludes.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

// =============================================================================
// LIFECYCLE MANAGEMENT
// =============================================================================

Renderer::~Renderer() {
    Shutdown();
}

void Renderer::Initialize(bool antialiasing, int antialiasing_samples, const wchar_t* charRamp, int charRampCount) {
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
    LoadCharCoverageFromJson(charRamp, charRampCount);

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

    if (!InitializeCharInfoTarget()) {
        Logger::Error("[Renderer] Failed to initialize CHAR_INFO render target");
        return;
    }

    if (!InitializeQuantizationShaders()) {
        Logger::Error("[Renderer] Failed to initialize quantization shaders");
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
    
    // Create resolved (non-MSAA) texture for quantization pass input (needs SRV)
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = _device->CreateTexture2D(&texDesc, nullptr, &_resolvedTexture);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create resolved texture: 0x" + ss.str());
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    hr = _device->CreateShaderResourceView(_resolvedTexture.Get(), &srvDesc, &_resolvedTextureSRV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create resolved texture SRV");
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
    // --- Point sampler: pixel-art (GUI, sprites) ---
    D3D11_SAMPLER_DESC pointDesc = {};
    pointDesc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    pointDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    // Slight positive LOD bias to favor slightly coarser mips (reduce aliasing).
    pointDesc.MipLODBias = 0.0f;
    pointDesc.MaxAnisotropy = 1;
    pointDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    pointDesc.MinLOD = 0;
    pointDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = _device->CreateSamplerState(&pointDesc, &_samplerLinear);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create point sampler: 0x" + ss.str());
        return false;
    }

    if (!CreateAnisotropicSampler()) {
        return false;
    }

    Logger::Debug("[Renderer] Created point sampler (pixel-art) and anisotropic sampler (terrain, " +
                  std::to_string(_maxAnisotropy) + "x)");
    return true;
}

bool Renderer::CreateAnisotropicSampler() {
    D3D11_SAMPLER_DESC anisoDesc = {};
    anisoDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    anisoDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    anisoDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    anisoDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    // Match point sampler: slight positive bias for terrain/aniso paths as well.
    anisoDesc.MipLODBias = 0.0f;
    anisoDesc.MaxAnisotropy = static_cast<UINT>(_maxAnisotropy);
    anisoDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    anisoDesc.MinLOD = 0;
    anisoDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = _device->CreateSamplerState(&anisoDesc, &_samplerAnisotropic);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create anisotropic sampler: 0x" + ss.str());
        return false;
    }
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
    // Create staging texture for CHAR_INFO readback (R16G16_UINT = glyph, attributes)
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = Screen::GetInst().GetWidth();
    stagingDesc.Height = Screen::GetInst().GetHeight();
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R16G16_UINT;
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

bool Renderer::InitializeCharInfoTarget() {
    const UINT w = Screen::GetInst().GetWidth();
    const UINT h = Screen::GetInst().GetHeight();

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = w;
    texDesc.Height = h;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R16G16_UINT;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;

    HRESULT hr = _device->CreateTexture2D(&texDesc, nullptr, &_charInfoTexture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create CHAR_INFO render target texture");
        return false;
    }

    hr = _device->CreateRenderTargetView(_charInfoTexture.Get(), nullptr, &_charInfoRTV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create CHAR_INFO RTV");
        return false;
    }

    return true;
}

namespace {

const char* QUANTIZATION_VS_SRC = R"(
struct VSIn {
    float2 pos : POSITION;
};
struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};
VSOut main(VSIn v) {
    VSOut o;
    o.pos = float4(v.pos.x, -v.pos.y, 0.0, 1.0);
    o.uv = (v.pos + 1.0) * 0.5;
    return o;
}
)";

const char* QUANTIZATION_PS_SRC = R"(
#include "ColorMonochrome.hlsl"

Texture2D<float4> g_colorTex : register(t0);
Texture1D<uint2>  g_lutTex    : register(t1);
SamplerState      g_sam       : register(s0);

cbuffer LUTConstants : register(b0) {
    float  Lmin;
    float  Lmax;
    uint   isMonochrome;
    uint   _pad;
};

static const uint BAYER4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

struct PSOut {
    uint2 glyph_attr : SV_Target0;
};

PSOut main(float4 pos : SV_Position, float2 uv : TEXCOORD0) {
    float3 rgb = g_colorTex.Sample(g_sam, uv).rgb;
    PSOut o;
    o.glyph_attr = uint2(0, 0);

    if (isMonochrome != 0u) {
        // g_colorTex is an sRGB RTV; sampling already returns linear RGB.
        float3 rgbLin = rgb;
        float L = dot(rgbLin, REC709);
        float denom = Lmax - Lmin;
        uint px = (uint)pos.x;
        uint py = (uint)pos.y;
        float bayerNorm = (float)(BAYER4[(py & 3u) * 4u + (px & 3u)]) / 16.0;
        float fIdx;
        if (denom < 1e-8) fIdx = 0.0;
        else if (L <= Lmin) fIdx = 0.0;
        else if (L >= Lmax) fIdx = 1023.0;
        else {
            float t = (L - Lmin) / denom;
            float fIdxCont = t * 1023.0;
            float base = floor(fIdxCont);
            float fracPart = frac(fIdxCont);
            fIdx = base + (fracPart > bayerNorm ? 1.0 : 0.0);
        }
        uint idx = (uint)clamp(fIdx, 0.0, 1023.0);
        o.glyph_attr = g_lutTex[idx];
    } else {
        uint r16 = (uint)(saturate(rgb.r) * 15.0 + 0.5);
        uint g16 = (uint)(saturate(rgb.g) * 15.0 + 0.5);
        uint b16 = (uint)(saturate(rgb.b) * 15.0 + 0.5);
        uint idx = r16 * 256u + g16 * 16u + b16;
        o.glyph_attr = g_lutTex[idx];
    }
    return o;
}
)";

} // namespace

bool Renderer::InitializeQuantizationShaders() {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errBlob = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif
    HRESULT hr = D3DCompile(QUANTIZATION_VS_SRC, strlen(QUANTIZATION_VS_SRC), nullptr, nullptr, nullptr,
                           "main", "vs_5_0", flags, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            Logger::Error("[Renderer] Quantization VS compile: " + std::string(static_cast<const char*>(errBlob->GetBufferPointer())));
            errBlob->Release();
        }
        return false;
    }
    if (errBlob) errBlob->Release();

    hr = _device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &_quantizationVS);
    if (FAILED(hr)) {
        vsBlob->Release();
        Logger::Error("[Renderer] Failed to create quantization VS");
        return false;
    }
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = _device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &_quantizationInputLayout);
    vsBlob->Release();
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create quantization input layout");
        return false;
    }

    ID3DBlob* psBlob = nullptr;
    errBlob = nullptr;
    ShaderIncludeMap quantIncludes;
    HLSLIncludes::AddToMap(quantIncludes);
    ShaderIncludeHandler quantIncludeHandler(&quantIncludes);
    hr = D3DCompile(QUANTIZATION_PS_SRC, strlen(QUANTIZATION_PS_SRC), nullptr, nullptr, &quantIncludeHandler,
                   "main", "ps_5_0", flags, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            Logger::Error("[Renderer] Quantization PS compile: " + std::string(static_cast<const char*>(errBlob->GetBufferPointer())));
            errBlob->Release();
        }
        return false;
    }
    if (errBlob) errBlob->Release();

    hr = _device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_quantizationPS);
    psBlob->Release();
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create quantization PS");
        return false;
    }

    // Fullscreen quad VB: 2 triangles (6 vertices), NDC positions (handled in VS by vertex ID)
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 6 * sizeof(float) * 2;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    float quadVerts[] = { -1,-1, 1,-1, -1,1,  -1,1, 1,-1, 1,1 };
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = quadVerts;
    hr = _device->CreateBuffer(&bd, &init, &_fullscreenQuadVB);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create fullscreen quad VB");
        return false;
    }

    return true;
}

bool Renderer::EnsureQuantizationResources() {
    if (_colorLUTState == ColorLUTState::NotComputed) return false;
    UploadLUTsToGPU();
    return true;
}

void Renderer::UploadLUTsToGPU() {
    if (_colorLUTState == ColorLUTState::NotComputed) return;
    const UINT multiSize = _rgbLUTDepth * _rgbLUTDepth * _rgbLUTDepth;
    const UINT monoSize = static_cast<UINT>(_monochromeLUTSize);

    std::vector<uint16_t> multiData(multiSize * 2);
    for (UINT i = 0; i < multiSize; ++i) {
        const CHAR_INFO& c = _colorLUT[i];
        multiData[i * 2 + 0] = static_cast<uint16_t>(c.Char.UnicodeChar & 0xFFFF);
        multiData[i * 2 + 1] = static_cast<uint16_t>(c.Attributes & 0xFFFF);
    }
    D3D11_TEXTURE1D_DESC tex1d = {};
    tex1d.Width = multiSize;
    tex1d.MipLevels = 1;
    tex1d.ArraySize = 1;
    tex1d.Format = DXGI_FORMAT_R16G16_UINT;
    tex1d.Usage = D3D11_USAGE_DEFAULT;
    tex1d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = multiData.data();
    _colorLUTTexture.Reset();
    HRESULT hr = _device->CreateTexture1D(&tex1d, &srd, &_colorLUTTexture);
    if (FAILED(hr)) { Logger::Error("[Renderer] Failed to create color LUT texture"); return; }
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16_UINT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    srvDesc.Texture1D.MipLevels = 1;
    _colorLUTSRV.Reset();
    _device->CreateShaderResourceView(_colorLUTTexture.Get(), &srvDesc, &_colorLUTSRV);

    std::vector<uint16_t> monoData(monoSize * 2);
    for (UINT i = 0; i < monoSize; ++i) {
        const CHAR_INFO& c = _monochromeLUT[i].second;
        monoData[i * 2 + 0] = static_cast<uint16_t>(c.Char.UnicodeChar & 0xFFFF);
        monoData[i * 2 + 1] = static_cast<uint16_t>(c.Attributes & 0xFFFF);
    }
    tex1d.Width = monoSize;
    srd.pSysMem = monoData.data();
    _monochromeLUTTexture.Reset();
    hr = _device->CreateTexture1D(&tex1d, &srd, &_monochromeLUTTexture);
    if (FAILED(hr)) { Logger::Error("[Renderer] Failed to create monochrome LUT texture"); return; }
    _monochromeLUTSRV.Reset();
    _device->CreateShaderResourceView(_monochromeLUTTexture.Get(), &srvDesc, &_monochromeLUTSRV);

    struct LutConstants {
        float Lmin, Lmax;
        uint32_t isMonochrome, pad;
    };
    LutConstants cb = {};
    cb.Lmin = _monochromeLUT.empty() ? 0.f : _monochromeLUT.front().first;
    cb.Lmax = _monochromeLUT.empty() ? 1.f : _monochromeLUT.back().first;
    cb.isMonochrome = (_colorLUTState == ColorLUTState::Monochrome) ? 1u : 0u;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(LutConstants);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    _lutConstantsCB.Reset();
    D3D11_SUBRESOURCE_DATA cbInit = {};
    cbInit.pSysMem = &cb;
    _device->CreateBuffer(&cbd, &cbInit, &_lutConstantsCB);
}

void Renderer::RunQuantizationPass() {
    if (_colorLUTState == ColorLUTState::NotComputed) {
        PrecomputeColorLUT();
    }
    if (_colorLUTState == ColorLUTState::NotComputed) return;
    EnsureQuantizationResources();
    if (!_resolvedTextureSRV || (!_colorLUTSRV && !_monochromeLUTSRV)) {
        Logger::Warning("[Renderer] RunQuantizationPass skipped: resolved SRV or LUT SRVs missing.");
        return;
    }

    const int w = Screen::GetInst().GetWidth();
    const int h = Screen::GetInst().GetHeight();

    _context->OMSetRenderTargets(1, _charInfoRTV.GetAddressOf(), nullptr);
    // Fullscreen quad shouldn't be culled or depth-tested (it writes uint2 into R16G16_UINT RT).
    _context->OMSetDepthStencilState(_depthStencilStateNoTest.Get(), 0);
    _context->RSSetState(_rasterizerStates[0].Get()); // solid, no cull, cw
    // No blending: overwrite with exact (glyph, attr) from LUT
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    _context->OMSetBlendState(_blendStateOpaque.Get(), blendFactor, 0xFFFFFFFF);

    // Clear to space (0x20) + default attribute (7) so any unrendered pixels are visible
    const float clearCharInfo[4] = { 32.0f, 7.0f, 0.0f, 0.0f };
    _context->ClearRenderTargetView(_charInfoRTV.Get(), clearCharInfo);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(w);
    vp.Height = static_cast<float>(h);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    _context->RSSetViewports(1, &vp);

    ID3D11ShaderResourceView* srvs[] = { _resolvedTextureSRV.Get(),
        (_colorLUTState == ColorLUTState::Monochrome) ? _monochromeLUTSRV.Get() : _colorLUTSRV.Get() };
    _context->PSSetShaderResources(0, 2, srvs);
    _context->PSSetSamplers(0, 1, _samplerLinear.GetAddressOf());
    _context->VSSetShader(_quantizationVS.Get(), nullptr, 0);
    _context->PSSetShader(_quantizationPS.Get(), nullptr, 0);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _context->IASetInputLayout(_quantizationInputLayout.Get());
    UINT stride = 2 * sizeof(float);
    UINT offset = 0;
    _context->IASetVertexBuffers(0, 1, _fullscreenQuadVB.GetAddressOf(), &stride, &offset);
    _context->VSSetConstantBuffers(0, 1, _lutConstantsCB.GetAddressOf());
    _context->PSSetConstantBuffers(0, 1, _lutConstantsCB.GetAddressOf());

    _context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    _context->PSSetShaderResources(0, 2, nullSRVs);

    // Restore main render target so next frame draws to the correct RT
    _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilView.Get());
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
    if (!_initialized) {
        Logger::Warning("[Renderer] DownloadFramebuffer called before initialization!");
        return;
    }

    const int width = Screen::GetInst().GetWidth();
    const int height = Screen::GetInst().GetHeight();
    auto& screen = Screen::GetInst();
    auto& pixelBuffer = screen.GetPixelBuffer();
    const size_t cellCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixelBuffer.size() != cellCount) {
        pixelBuffer.resize(cellCount);
    }

    // Ensure quantization draw has completed before copying
    _context->Flush();

    // Copy CHAR_INFO render target to staging (R16G16_UINT = glyph, attributes per pixel)
    _context->CopyResource(_stagingTexture.Get(), _charInfoTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = _context->Map(_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        Logger::Warning("[Renderer] Failed to map staging texture for CHAR_INFO readback");
        return;
    }

    const size_t rowPitch = mapped.RowPitch;
    CHAR_INFO* dest = pixelBuffer.data();

    for (int y = 0; y < height; ++y) {
        const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + y * rowPitch);
        for (int x = 0; x < width; ++x) {
            dest->Char.UnicodeChar = static_cast<wchar_t>(row[x * 2 + 0]);
            dest->Attributes = static_cast<WORD>(row[x * 2 + 1]);
            ++dest;
        }
    }

    _context->Unmap(_stagingTexture.Get(), 0);

#if 1
    // Debug: log CHAR_INFO readback (expect 65,15 when shader forces 'A')
    {
        const CHAR_INFO* pb = pixelBuffer.data();
        Logger::Debug("[Renderer] CHAR_INFO readback: center=(" +
            std::to_string((unsigned)pb[height/2 * width + width/2].Char.UnicodeChar) + "," +
            std::to_string((unsigned)pb[height/2 * width + width/2].Attributes) + ") top-left=(" +
            std::to_string((unsigned)pb[0].Char.UnicodeChar) + "," + std::to_string((unsigned)pb[0].Attributes) + ")");
    }
#endif
}

// =========================================================================
// Cleanup
// =========================================================================

void Renderer::Shutdown()
{
    if (!_initialized) return;

    // Release all COM objects (ComPtr handles this automatically)
    _textureCache.clear();
    _currentTextureSRV.Reset();
    _fullscreenQuadVB.Reset();
    _quantizationInputLayout.Reset();
    _quantizationPS.Reset();
    _quantizationVS.Reset();
    _lutConstantsCB.Reset();
    _monochromeLUTSRV.Reset();
    _colorLUTSRV.Reset();
    _monochromeLUTTexture.Reset();
    _colorLUTTexture.Reset();
    _resolvedTextureSRV.Reset();
    _charInfoRTV.Reset();
    _charInfoTexture.Reset();
    _stagingTexture.Reset();

    _samplerLinear.Reset();
    _samplerAnisotropic.Reset();

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

