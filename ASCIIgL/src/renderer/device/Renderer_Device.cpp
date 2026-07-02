#include <ASCIIgL/renderer/Renderer.hpp>

#include <algorithm>              // std::clamp
#include <cstring>                // strlen
#include <sstream>                // std::ostringstream
#include <string>                 // std::wstring

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <ASCIIgL/renderer/HLSLIncludes.hpp>
#include <ASCIIgL/renderer/Shader.hpp>
#include <ASCIIgL/renderer/screen/Screen.hpp>
#include <ASCIIgL/util/Logger.hpp>
#include <ASCIIgL/util/CoverageJson.hpp>
#include <ASCIIgL/util/FontAtlasBuilder.hpp>

#include "renderer/resources/ShaderCompilerInternal.hpp"
#include "renderer/core/RendererImpl.hpp"

namespace ASCIIgL {

using ColorLUTState = Renderer::Impl::ColorLUTState;

// =============================================================================
// LIFECYCLE MANAGEMENT
// =============================================================================

Renderer::~Renderer() {
    Shutdown();
}

void Renderer::Initialize(const wchar_t* charRamp, int charRampCount) {
    if (!impl_->_initialized) {
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

    if (!Screen::GetInst().IsRenderToTerminal()) {
        if (!InitializeFontAtlas()) {
            Logger::Error("[Renderer] Failed to initialize font atlas (window mode)");
            return;
        }

        if (!InitializeWindowSwapChain()) {
            Logger::Error("[Renderer] Failed to initialize window swap chain (window mode)");
            return;
        }

        if (!InitializeAsciiWindowPass()) {
            Logger::Error("[Renderer] Failed to initialize ASCII window pass (window mode)");
            return;
        }
    }

    // Pipeline state (depth + blend) is set in BeginColBuffFrame when RTV is bound.

    impl_->_initialized = true;
    Logger::Debug("Renderer initialization complete - Device, Shaders, Buffers ready");
}

bool Renderer::IsInitialized() const {
    return impl_->_initialized;
}

ID3D11Device* Renderer::GetD3D11Device() const {
    return impl_ ? impl_->_device.Get() : nullptr;
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
        impl_->_device.GetAddressOf(),  // Output device
        &featureLevel,              // Chosen feature level
        impl_->_context.GetAddressOf()  // Output context
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
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = Screen::GetInst().GetWidth();
    texDesc.Height = Screen::GetInst().GetHeight();
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;

    HRESULT hr = impl_->_device->CreateTexture2D(&texDesc, nullptr, &impl_->_renderTarget);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create render target texture: 0x" + ss.str());
        return false;
    }

    hr = impl_->_device->CreateRenderTargetView(impl_->_renderTarget.Get(), nullptr, &impl_->_renderTargetView);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create render target view: 0x" + ss.str());
        return false;
    }

    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    impl_->_resolvedTexture.Reset();
    impl_->_resolvedTextureSRV.Reset();
    hr = impl_->_device->CreateTexture2D(&texDesc, nullptr, &impl_->_resolvedTexture);
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
    hr = impl_->_device->CreateShaderResourceView(impl_->_resolvedTexture.Get(), &srvDesc, &impl_->_resolvedTextureSRV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create resolved texture SRV");
        return false;
    }

    Logger::Info("[Renderer] Render target initialized: " +
        std::to_string(Screen::GetInst().GetWidth()) + "x" +
        std::to_string(Screen::GetInst().GetHeight()));

    return true;
}

bool Renderer::InitializeDepthStencil() {
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = Screen::GetInst().GetWidth();
    depthDesc.Height = Screen::GetInst().GetHeight();
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    impl_->_depthStencilView.Reset();
    impl_->_depthStencilBuffer.Reset();
    HRESULT hr = impl_->_device->CreateTexture2D(&depthDesc, nullptr, &impl_->_depthStencilBuffer);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create depth stencil buffer: 0x" + ss.str());
        return false;
    }

    hr = impl_->_device->CreateDepthStencilView(impl_->_depthStencilBuffer.Get(), nullptr, &impl_->_depthStencilView);
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

    hr = impl_->_device->CreateDepthStencilState(&dsDesc, &impl_->_depthStencilState);
    if (FAILED(hr)) {
        std::ostringstream ss;
        ss << std::hex << hr;
        Logger::Error("[Renderer] Failed to create depth stencil state: 0x" + ss.str());
        return false;
    }

    // No-depth state (depth test disabled)
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = impl_->_device->CreateDepthStencilState(&dsDesc, &impl_->_depthStencilStateNoTest);
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
    hr = impl_->_device->CreateDepthStencilState(&dsDesc, &impl_->_depthStencilStateNoWrite);
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

    HRESULT hr = impl_->_device->CreateSamplerState(&pointDesc, &impl_->_samplerLinear);
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
                  std::to_string(impl_->_maxAnisotropy) + "x)");
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
    anisoDesc.MaxAnisotropy = static_cast<UINT>(impl_->_maxAnisotropy);
    anisoDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    anisoDesc.MinLOD = 0;
    anisoDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = impl_->_device->CreateSamplerState(&anisoDesc, &impl_->_samplerAnisotropic);
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

        HRESULT hr = impl_->_device->CreateRasterizerState(&desc, &impl_->_rasterizerStates[i]);
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

    HRESULT hr = impl_->_device->CreateBlendState(&opaqueDesc, &impl_->_blendStateOpaque);
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

    hr = impl_->_device->CreateBlendState(&alphaDesc, &impl_->_blendStateAlpha);
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

    HRESULT hr = impl_->_device->CreateTexture2D(&stagingDesc, nullptr, &impl_->_stagingTexture);
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
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    HRESULT hr = impl_->_device->CreateTexture2D(&texDesc, nullptr, &impl_->_charInfoTexture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create CHAR_INFO render target texture");
        return false;
    }

    hr = impl_->_device->CreateRenderTargetView(impl_->_charInfoTexture.Get(), nullptr, &impl_->_charInfoRTV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create CHAR_INFO RTV");
        return false;
    }

    // SRV for ASCII-to-window pass (sample CHAR_INFO as uint2)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16_UINT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    impl_->_charInfoSRV.Reset();
    hr = impl_->_device->CreateShaderResourceView(impl_->_charInfoTexture.Get(), &srvDesc, &impl_->_charInfoSRV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create CHAR_INFO SRV");
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
#include "ColorUtil.hlsl"

Texture2D<float4> g_colorTex : register(t0);
Texture1D<uint2>  g_lutTex    : register(t1);
SamplerState      g_sam       : register(s0);

cbuffer LUTConstants : register(b0) {
    float  Lmin;
    float  Lmax;
    uint   isMonochrome;
    uint   ditherEnabled;
};

static const uint BAYER4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

struct PSOut {
    uint2 glyph_attr : SV_Target0;
};

PSOut main(float4 pos : SV_Position, float2 uv : TEXCOORD0) {
    float3 rgb = g_colorTex.Load(int3((int)pos.x, (int)pos.y, 0)).rgb;
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
            if (ditherEnabled != 0u) {
                float base = floor(fIdxCont);
                float fracPart = frac(fIdxCont);
                fIdx = base + (fracPart > bayerNorm ? 1.0 : 0.0);
            } else {
                fIdx = floor(fIdxCont + 0.5);
            }
        }
        uint idx = (uint)clamp(fIdx, 0.0, 1023.0);
        o.glyph_attr = g_lutTex[idx];
    } else {
        float3 srgb = linearToSRGB(rgb);
        float3 cont = saturate(srgb) * 15.0;  // continuous [0.0, 15.0] per channel

        uint r16, g16, b16;

        if (ditherEnabled != 0u) {
            uint px = (uint)pos.x;
            uint py = (uint)pos.y;
            float bayerNorm = (float)(BAYER4[(py & 3u) * 4u + (px & 3u)]) / 16.0;

            float3 base = floor(cont);
            float3 fr   = cont - base;  // per-channel fractional part

            float3 dithered = base + float3(
                fr.r > bayerNorm ? 1.0 : 0.0,
                fr.g > bayerNorm ? 1.0 : 0.0,
                fr.b > bayerNorm ? 1.0 : 0.0
            );

            r16 = (uint)clamp(dithered.r, 0.0, 15.0);
            g16 = (uint)clamp(dithered.g, 0.0, 15.0);
            b16 = (uint)clamp(dithered.b, 0.0, 15.0);
        } else {
            // Original: round to nearest
            r16 = (uint)(cont.r + 0.5);
            g16 = (uint)(cont.g + 0.5);
            b16 = (uint)(cont.b + 0.5);
        }

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

    hr = impl_->_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &impl_->_quantizationVS);
    if (FAILED(hr)) {
        vsBlob->Release();
        Logger::Error("[Renderer] Failed to create quantization VS");
        return false;
    }
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = impl_->_device->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &impl_->_quantizationInputLayout);
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

    hr = impl_->_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &impl_->_quantizationPS);
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
    hr = impl_->_device->CreateBuffer(&bd, &init, &impl_->_fullscreenQuadVB);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create fullscreen quad VB");
        return false;
    }

    return true;
}

bool Renderer::EnsureQuantizationResources() {
    if (impl_->_colorLUTState == ColorLUTState::NotComputed) return false;
    if (impl_->_lutGpuResourcesDirty || !impl_->_lutConstantsCB ||
        !impl_->_colorLUTTexture || !impl_->_monochromeLUTTexture ||
        !impl_->_colorLUTSRV || !impl_->_monochromeLUTSRV) {
        UploadLUTsToGPU();
    }
    return true;
}

void Renderer::UploadLUTsToGPU() {
    if (impl_->_colorLUTState == ColorLUTState::NotComputed) return;
    const UINT multiSize = Renderer::Impl::_rgbLUTDepth * Renderer::Impl::_rgbLUTDepth * Renderer::Impl::_rgbLUTDepth;
    const UINT monoSize = static_cast<UINT>(Renderer::Impl::_monochromeLUTSize);

    std::vector<uint16_t> multiData(multiSize * 2);
    for (UINT i = 0; i < multiSize; ++i) {
        const ScreenPixel& c = impl_->_colorLUT[i];
        multiData[i * 2 + 0] = static_cast<uint16_t>(c.glyph & 0xFFFF);
        multiData[i * 2 + 1] = static_cast<uint16_t>(c.attributes & 0xFFFF);
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
    impl_->_colorLUTTexture.Reset();
    HRESULT hr = impl_->_device->CreateTexture1D(&tex1d, &srd, &impl_->_colorLUTTexture);
    if (FAILED(hr)) { Logger::Error("[Renderer] Failed to create color LUT texture"); return; }
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16_UINT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    srvDesc.Texture1D.MipLevels = 1;
    impl_->_colorLUTSRV.Reset();
    impl_->_device->CreateShaderResourceView(impl_->_colorLUTTexture.Get(), &srvDesc, &impl_->_colorLUTSRV);

    std::vector<uint16_t> monoData(monoSize * 2);
    for (UINT i = 0; i < monoSize; ++i) {
        const ScreenPixel& c = impl_->_monochromeLUT[i].second;
        monoData[i * 2 + 0] = static_cast<uint16_t>(c.glyph & 0xFFFF);
        monoData[i * 2 + 1] = static_cast<uint16_t>(c.attributes & 0xFFFF);
    }
    tex1d.Width = monoSize;
    srd.pSysMem = monoData.data();
    impl_->_monochromeLUTTexture.Reset();
    hr = impl_->_device->CreateTexture1D(&tex1d, &srd, &impl_->_monochromeLUTTexture);
    if (FAILED(hr)) { Logger::Error("[Renderer] Failed to create monochrome LUT texture"); return; }
    impl_->_monochromeLUTSRV.Reset();
    impl_->_device->CreateShaderResourceView(impl_->_monochromeLUTTexture.Get(), &srvDesc, &impl_->_monochromeLUTSRV);

    struct LutConstants {
        float Lmin, Lmax;
        uint32_t isMonochrome, ditherEnabled;
    };
    LutConstants cb = {};
    cb.Lmin = impl_->_monochromeLUT.empty() ? 0.f : impl_->_monochromeLUT.front().first;
    cb.Lmax = impl_->_monochromeLUT.empty() ? 1.f : impl_->_monochromeLUT.back().first;
    cb.isMonochrome = (impl_->_colorLUTState == ColorLUTState::Monochrome) ? 1u : 0u;
    cb.ditherEnabled = impl_->_ditheringEnabled ? 1u : 0u;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(LutConstants);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    impl_->_lutConstantsCB.Reset();
    D3D11_SUBRESOURCE_DATA cbInit = {};
    cbInit.pSysMem = &cb;
    impl_->_device->CreateBuffer(&cbd, &cbInit, &impl_->_lutConstantsCB);
    impl_->_lutGpuResourcesDirty = false;
}

void Renderer::RunQuantizationPass() {
    InvalidateBoundState();

    if (impl_->_colorLUTState == ColorLUTState::NotComputed) {
        PrecomputeColorLUT();
    }
    if (impl_->_colorLUTState == ColorLUTState::NotComputed) return;
    EnsureQuantizationResources();
    if (!impl_->_resolvedTextureSRV || (!impl_->_colorLUTSRV && !impl_->_monochromeLUTSRV)) {
        Logger::Warning("[Renderer] RunQuantizationPass skipped: resolved SRV or LUT SRVs missing.");
        return;
    }

    const int w = Screen::GetInst().GetWidth();
    const int h = Screen::GetInst().GetHeight();

    impl_->_context->OMSetRenderTargets(1, impl_->_charInfoRTV.GetAddressOf(), nullptr);
    // Fullscreen quad shouldn't be culled or depth-tested (it writes uint2 into R16G16_UINT RT).
    impl_->_context->OMSetDepthStencilState(impl_->_depthStencilStateNoTest.Get(), 0);
    impl_->_context->RSSetState(impl_->_rasterizerStates[0].Get()); // solid, no cull, cw
    // No blending: overwrite with exact (glyph, attr) from LUT
    const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    impl_->_context->OMSetBlendState(impl_->_blendStateOpaque.Get(), blendFactor, 0xFFFFFFFF);

    // Clear to space (0x20) + default attribute (7) so any unrendered pixels are visible
    const float clearCharInfo[4] = { 32.0f, 7.0f, 0.0f, 0.0f };
    impl_->_context->ClearRenderTargetView(impl_->_charInfoRTV.Get(), clearCharInfo);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(w);
    vp.Height = static_cast<float>(h);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    impl_->_context->RSSetViewports(1, &vp);

    ID3D11ShaderResourceView* srvs[] = { impl_->_resolvedTextureSRV.Get(),
        (impl_->_colorLUTState == ColorLUTState::Monochrome) ? impl_->_monochromeLUTSRV.Get() : impl_->_colorLUTSRV.Get() };
    impl_->_context->PSSetShaderResources(0, 2, srvs);
    impl_->_context->PSSetSamplers(0, 1, impl_->_samplerLinear.GetAddressOf());
    impl_->_context->VSSetShader(impl_->_quantizationVS.Get(), nullptr, 0);
    impl_->_context->PSSetShader(impl_->_quantizationPS.Get(), nullptr, 0);
    impl_->_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    impl_->_context->IASetInputLayout(impl_->_quantizationInputLayout.Get());
    UINT stride = 2 * sizeof(float);
    UINT offset = 0;
    impl_->_context->IASetVertexBuffers(0, 1, impl_->_fullscreenQuadVB.GetAddressOf(), &stride, &offset);
    impl_->_context->VSSetConstantBuffers(0, 1, impl_->_lutConstantsCB.GetAddressOf());
    impl_->_context->PSSetConstantBuffers(0, 1, impl_->_lutConstantsCB.GetAddressOf());

    impl_->_context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    impl_->_context->PSSetShaderResources(0, 2, nullSRVs);

    // Restore main render target so next frame draws to the correct RT
    impl_->_context->OMSetRenderTargets(1, impl_->_renderTargetView.GetAddressOf(), impl_->_depthStencilView.Get());
    InvalidateBoundState();
}

// Builds the font atlas once using current Screen font size and CoverageJson cell size.
// If font size can change at runtime, call this again (or expose a "rebuild atlas" path) when it does.
bool Renderer::InitializeFontAtlas() {
    float fontSize = Screen::GetInst().GetFontSize();
    int cellPixelsX = 0, cellPixelsY = 0;
    if (!CoverageJson::GetCellSizeForFontSize(fontSize, &cellPixelsX, &cellPixelsY)) {
        Logger::Error("[Renderer] Font atlas: could not get cell size for font size");
        return false;
    }
    // Use the active char ramp already parsed in Initialize (LoadCharCoverageFromJson)
    if (impl_->_charRamp.empty()) {
        Logger::Error("[Renderer] Font atlas: no char ramp (LoadCharCoverageFromJson did not populate impl_->_charRamp)");
        return false;
    }
    std::wstring rampStr(impl_->_charRamp.begin(), impl_->_charRamp.end());
    FontAtlasBuildResult result = BuildFontAtlasFromDirectWrite(nullptr, fontSize, rampStr.c_str(), cellPixelsX, cellPixelsY);
    if (!result.success || result.slices.empty()) {
        Logger::Error("[Renderer] Font atlas: BuildFontAtlasFromDirectWrite failed");
        return false;
    }

    const UINT width = static_cast<UINT>(result.cellPixelsX);
    const UINT height = static_cast<UINT>(result.cellPixelsY);
    const UINT arraySize = static_cast<UINT>(result.glyphCount);

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = arraySize;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    impl_->_fontAtlasTexture.Reset();
    HRESULT hr = impl_->_device->CreateTexture2D(&texDesc, nullptr, &impl_->_fontAtlasTexture);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Font atlas: failed to create Texture2DArray");
        return false;
    }

    const UINT rowPitch = width * 4;
    const UINT slicePitch = rowPitch * height;
    for (UINT i = 0; i < arraySize && i < result.slices.size(); ++i) {
        const std::vector<std::uint8_t>& slice = result.slices[i];
        if (slice.size() < slicePitch) continue;
        D3D11_BOX box = { 0, 0, 0, width, height, 1 };
        impl_->_context->UpdateSubresource(impl_->_fontAtlasTexture.Get(), i, &box, slice.data(), rowPitch, slicePitch);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = arraySize;
    impl_->_fontAtlasSRV.Reset();
    hr = impl_->_device->CreateShaderResourceView(impl_->_fontAtlasTexture.Get(), &srvDesc, &impl_->_fontAtlasSRV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Font atlas: failed to create SRV");
        impl_->_fontAtlasTexture.Reset();
        return false;
    }

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.MaxAnisotropy = 1;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    impl_->_fontAtlasSamplerPoint.Reset();
    hr = impl_->_device->CreateSamplerState(&sampDesc, &impl_->_fontAtlasSamplerPoint);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Font atlas: failed to create point sampler");
        impl_->_fontAtlasSRV.Reset();
        impl_->_fontAtlasTexture.Reset();
        return false;
    }

    impl_->_fontAtlasCellPixelsX = result.cellPixelsX;
    impl_->_fontAtlasCellPixelsY = result.cellPixelsY;
    impl_->_fontAtlasGlyphCount = static_cast<int>(arraySize);
    Logger::Debug("[Renderer] Font atlas created: " + std::to_string(width) + "x" + std::to_string(height) + " x " + std::to_string(arraySize) + " slices");
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
    if (FAILED(impl_->_device.As(&dxgiDevice))) {
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

    if (FAILED(dxgiFactory->CreateSwapChain(impl_->_device.Get(), &swapChainDesc, &impl_->_debugSwapChain))) {
        Logger::Error("[Renderer] Failed to create debug swap chain");
        return false;
    }

    Logger::Debug("[Renderer] Debug swap chain created successfully");
    return true;
}

bool Renderer::InitializeWindowSwapChain() {
    // Use Screen's window handle (console in terminal mode, app window in window mode).
    HWND hwnd = static_cast<HWND>(Screen::GetInst().GetWindowHandle());
    if (!hwnd) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: Screen window handle is null");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    // Width/Height 0 -> use current window client size.
    swapChainDesc.BufferDesc.Width = 0;
    swapChainDesc.BufferDesc.Height = 0;
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
    if (FAILED(impl_->_device.As(&dxgiDevice))) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: failed to get DXGI device");
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: failed to get DXGI adapter");
        return false;
    }

    ComPtr<IDXGIFactory> dxgiFactory;
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory))) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: failed to get DXGI factory");
        return false;
    }

    impl_->_windowSwapChain.Reset();
    HRESULT hr = dxgiFactory->CreateSwapChain(impl_->_device.Get(), &swapChainDesc, &impl_->_windowSwapChain);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: failed to create window swap chain");
        return false;
    }

    // Create RTV for the window back buffer.
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = impl_->_windowSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: failed to get back buffer");
        impl_->_windowSwapChain.Reset();
        return false;
    }

    // Create RTV with sRGB format so the GPU applies linear->sRGB on write. Swap chain buffer stays UNORM.
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    impl_->_windowRTV.Reset();
    hr = impl_->_device->CreateRenderTargetView(backBuffer.Get(), &rtvDesc, &impl_->_windowRTV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] InitializeWindowSwapChain: failed to create back buffer RTV");
        impl_->_windowSwapChain.Reset();
        return false;
    }

    Logger::Debug("[Renderer] Window swap chain created successfully");
    return true;
}

// =========================================================================
// Framebuffer Download (GPU -> CPU)
// =========================================================================

void Renderer::DownloadFramebuffer()
{
    if (!impl_->_initialized) {
        Logger::Warning("[Renderer] DownloadFramebuffer called before initialization!");
        return;
    }

    const int width = Screen::GetInst().GetWidth();
    const int height = Screen::GetInst().GetHeight();
    auto& screen = Screen::GetInst();
    const size_t cellCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    ScreenPixel* pixelBuffer = screen.GetPixelBufferData();
    if (!pixelBuffer || screen.GetPixelBufferSize() != cellCount) {
        Logger::Warning("[Renderer] Screen pixel buffer is unavailable or incorrectly sized");
        return;
    }

    // Copy CHAR_INFO render target to staging (R16G16_UINT = glyph, attributes per pixel)
    impl_->_context->CopyResource(impl_->_stagingTexture.Get(), impl_->_charInfoTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = impl_->_context->Map(impl_->_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        Logger::Warning("[Renderer] Failed to map staging texture for CHAR_INFO readback");
        return;
    }

    const size_t rowPitch = mapped.RowPitch;
    ScreenPixel* dest = pixelBuffer;

    for (int y = 0; y < height; ++y) {
        const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + y * rowPitch);
        for (int x = 0; x < width; ++x) {
            dest->glyph = static_cast<wchar_t>(row[x * 2 + 0]);
            dest->attributes = static_cast<uint16_t>(row[x * 2 + 1]);
            ++dest;
        }
    }

    impl_->_context->Unmap(impl_->_stagingTexture.Get(), 0);
}

// =========================================================================
// Cleanup
// =========================================================================

void Renderer::Shutdown()
{
    if (!impl_->_initialized) return;

    // Release all COM objects (ComPtr handles this automatically)
    impl_->_textureCache.clear();
    impl_->_currentTextureSRV.Reset();
    impl_->_fullscreenQuadVB.Reset();
    impl_->_quantizationInputLayout.Reset();
    impl_->_quantizationPS.Reset();
    impl_->_quantizationVS.Reset();
    impl_->_lutConstantsCB.Reset();
    impl_->_monochromeLUTSRV.Reset();
    impl_->_colorLUTSRV.Reset();
    impl_->_monochromeLUTTexture.Reset();
    impl_->_colorLUTTexture.Reset();
    impl_->_resolvedTextureSRV.Reset();
    impl_->_charInfoRTV.Reset();
    impl_->_charInfoSRV.Reset();
    impl_->_charInfoTexture.Reset();
    impl_->_stagingTexture.Reset();

    impl_->_fontAtlasSamplerPoint.Reset();
    impl_->_fontAtlasSRV.Reset();
    impl_->_fontAtlasTexture.Reset();

    impl_->_asciiWindowPS.Reset();
    impl_->_asciiWindowCB.Reset();
    impl_->_paletteSRVWindow.Reset();
    impl_->_paletteBufferWindow.Reset();
    impl_->_rampLookupSRV.Reset();
    impl_->_rampLookupBuffer.Reset();

    impl_->_windowRTV.Reset();
    impl_->_windowSwapChain.Reset();

    impl_->_samplerLinear.Reset();
    impl_->_samplerAnisotropic.Reset();

    // Release all rasterizer states
    for (auto& state : impl_->_rasterizerStates) {
        state.Reset();
    }

    impl_->_blendStateAlpha.Reset();
    impl_->_blendStateOpaque.Reset();
    impl_->_depthStencilStateNoWrite.Reset();
    impl_->_depthStencilStateNoTest.Reset();
    impl_->_depthStencilState.Reset();
    impl_->_depthStencilView.Reset();
    impl_->_depthStencilBuffer.Reset();
    impl_->_renderTargetView.Reset();
    impl_->_resolvedTexture.Reset();
    impl_->_renderTarget.Reset();
    impl_->_context.Reset();
    impl_->_device.Reset();

    impl_->_initialized = false;
    Logger::Debug("[Renderer] Shutdown complete");
}

} // namespace ASCIIgL

