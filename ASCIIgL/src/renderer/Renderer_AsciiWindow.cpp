#include <ASCIIgL/renderer/Renderer.hpp>

#include <cstdint>
#include <cstring>      // strlen
#include <vector>

#include <d3dcompiler.h>

#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/util/Logger.hpp>

namespace ASCIIgL {

bool Renderer::InitializeAsciiWindowPass() {
    // Simple passthrough VS (we reuse _quantizationVS and _fullscreenQuadVB) and dedicated PS for ASCII->window.
    static const char* ASCII_WINDOW_PS_SRC = R"(
cbuffer AsciiWindowCB : register(b0)
{
    uint2 g_cells;        // cellsX, cellsY
    uint2 g_cellPixels;   // cellPixelsX, cellPixelsY
    uint2 g_outputPixels; // outWidth, outHeight
    uint g_atlasSliceCount; // font atlas array size (ramp length)
    uint g_pad0;
};

Texture2D<uint2>       g_charInfo  : register(t0); // glyph (Unicode), attrs
Texture2DArray<float4> g_fontAtlas : register(t1);  // RGBA, alpha in A; slice = ramp index
StructuredBuffer<float4> g_palette : register(t2);  // 16 palette entries RGBA (sRGB 0-1)
StructuredBuffer<uint> g_rampLookup : register(t3); // Unicode code point (0..255) -> ramp index for atlas slice

SamplerState g_samplerPoint  : register(s0);
SamplerState g_samplerLinear : register(s1);

// Palette is sRGB 0-1; decode to linear for blending.
float3 sRGBToLinear(float3 c) {
    float3 lo = c / 12.92;
    float3 hi = pow(max((c + 0.055) / 1.055, 0.0), 2.4);
    return lerp(lo, hi, step(0.04045, c));
}

float4 main(float4 svPos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    uint2 pix = uint2(svPos.xy);

    uint2 cell = pix / g_cellPixels;
    uint2 inCell = pix % g_cellPixels;

    cell = min(cell, g_cells - 1);

    uint2 ci = g_charInfo.Load(int3(cell, 0));
    // R16G16_UINT: .x = glyph, .y = attr. Mask to 16-bit in case driver packs or extends differently than CPU readback.
    uint unicodeChar = ci.x & 0xFFFFu;
    uint attrs       = ci.y & 0xFFFFu;

    // Font atlas slices are indexed by ramp order (0..N-1), not by Unicode. Look up ramp index.
    uint lookupIdx = min(unicodeChar, 255u);
    uint rampIndex = g_rampLookup[lookupIdx];
    if (g_atlasSliceCount > 0u)
        rampIndex = min(rampIndex, g_atlasSliceCount - 1u); // clamp to valid slice

    uint fgIndex = attrs & 0x0F;
    uint bgIndex = (attrs >> 4) & 0x0F;

    float2 atlasUV = (float2(inCell) + 0.5) / float2(g_cellPixels);
    float4 glyphSample = g_fontAtlas.SampleLevel(g_samplerPoint, float3(atlasUV, rampIndex), 0.0);
    float alpha = (glyphSample.r + glyphSample.g + glyphSample.b) / 3.0;

    // Palette has 16 entries (sRGB 0-1). Decode to linear, blend in linear, output linear (RTV does linear->sRGB on write).
    float3 fgSrgb = g_palette[fgIndex].rgb;
    float3 bgSrgb = g_palette[bgIndex].rgb;
    float3 fg = sRGBToLinear(fgSrgb);
    float3 bg = sRGBToLinear(bgSrgb);
    float3 rgbLinear = lerp(bg, fg, alpha);

    return float4(saturate(rgbLinear), 1.0);
}
)";

    // Compile pixel shader (debug + skip opt in _DEBUG for RenderDoc shader debugging)
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errBlob;
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr = D3DCompile(
        ASCII_WINDOW_PS_SRC,
        std::strlen(ASCII_WINDOW_PS_SRC),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        compileFlags,
        0,
        &psBlob,
        &errBlob
    );
    if (FAILED(hr)) {
        if (errBlob) {
            Logger::Error("[Renderer] Failed to compile ASCII window PS: " + std::string((char*)errBlob->GetBufferPointer()));
        } else {
            Logger::Error("[Renderer] Failed to compile ASCII window PS.");
        }
        return false;
    }

    _asciiWindowPS.Reset();
    hr = _device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_asciiWindowPS);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create ASCII window pixel shader");
        return false;
    }

    // Create constant buffer (must match HLSL AsciiWindowCB)
    struct AsciiWindowCB {
        UINT cellsX, cellsY;
        UINT cellPixelsX, cellPixelsY;
        UINT outWidth, outHeight;
        UINT atlasSliceCount, pad0;
    };

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(AsciiWindowCB);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    _asciiWindowCB.Reset();
    hr = _device->CreateBuffer(&cbd, nullptr, &_asciiWindowCB);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create ASCII window constant buffer");
        return false;
    }

    // Create palette structured buffer (16 x float4) from current Screen palette
    Palette& palette = Screen::GetInst().GetPalette();
    const UINT paletteSize = Palette::COLOR_COUNT;
    constexpr UINT paletteStride = sizeof(float) * 4;
    const UINT paletteByteWidth = paletteSize * paletteStride;
    std::vector<float> paletteData(paletteSize * 4);
    for (UINT i = 0; i < paletteSize; ++i) {
        glm::vec3 n = palette.GetRGBNormalized(i);
        paletteData[i * 4 + 0] = n.r;
        paletteData[i * 4 + 1] = n.g;
        paletteData[i * 4 + 2] = n.b;
        paletteData[i * 4 + 3] = 1.0f;
    }

    D3D11_BUFFER_DESC bufDesc = {};
    bufDesc.ByteWidth = paletteByteWidth;
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufDesc.StructureByteStride = paletteStride;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = paletteData.data();
    initData.SysMemPitch = paletteByteWidth;
    initData.SysMemSlicePitch = 0;

    _paletteBufferWindow.Reset();
    hr = _device->CreateBuffer(&bufDesc, &initData, &_paletteBufferWindow);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create window palette buffer");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc = {};
    paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    paletteSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    paletteSrvDesc.Buffer.FirstElement = 0;
    paletteSrvDesc.Buffer.NumElements = paletteSize;

    _paletteSRVWindow.Reset();
    hr = _device->CreateShaderResourceView(_paletteBufferWindow.Get(), &paletteSrvDesc, &_paletteSRVWindow);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create window palette SRV");
        return false;
    }

    // Ramp lookup: map Unicode code point (0..255) -> ramp index (font atlas slice)
    // Must use the same _charRamp order as the font atlas (built in InitializeFontAtlas).
    if (_charRamp.size() != static_cast<size_t>(_fontAtlasGlyphCount)) {
        Logger::Error("[Renderer] ASCII window: char ramp size (" + std::to_string(_charRamp.size()) +
            ") != font atlas glyph count (" + std::to_string(_fontAtlasGlyphCount) + "); glyphs will be wrong.");
    }
    constexpr UINT rampLookupSize = 256;
    std::vector<uint32_t> rampLookupData(rampLookupSize, 0u);
    for (UINT cp = 0; cp < rampLookupSize; ++cp) {
        for (size_t j = 0; j < _charRamp.size(); ++j) {
            if (static_cast<unsigned>(_charRamp[j]) == cp) {
                rampLookupData[cp] = static_cast<uint32_t>(j);
                break;
            }
        }
    }
    Logger::Debug("[Renderer] Ramp lookup built: " + std::to_string(_charRamp.size()) + " glyphs, atlas slices " + std::to_string(_fontAtlasGlyphCount));

    constexpr UINT rampLookupStride = sizeof(uint32_t);
    const UINT rampLookupByteWidth = rampLookupSize * rampLookupStride;

    D3D11_BUFFER_DESC rampLookupBufDesc = {};
    rampLookupBufDesc.ByteWidth = rampLookupByteWidth;
    rampLookupBufDesc.Usage = D3D11_USAGE_DEFAULT;
    rampLookupBufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    rampLookupBufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    rampLookupBufDesc.StructureByteStride = rampLookupStride;

    D3D11_SUBRESOURCE_DATA rampLookupInit = {};
    rampLookupInit.pSysMem = rampLookupData.data();
    rampLookupInit.SysMemPitch = rampLookupByteWidth;
    rampLookupInit.SysMemSlicePitch = 0;

    _rampLookupBuffer.Reset();
    hr = _device->CreateBuffer(&rampLookupBufDesc, &rampLookupInit, &_rampLookupBuffer);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create ramp lookup buffer");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC rampLookupSrvDesc = {};
    rampLookupSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    rampLookupSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    rampLookupSrvDesc.Buffer.FirstElement = 0;
    rampLookupSrvDesc.Buffer.NumElements = rampLookupSize;

    _rampLookupSRV.Reset();
    hr = _device->CreateShaderResourceView(_rampLookupBuffer.Get(), &rampLookupSrvDesc, &_rampLookupSRV);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create ramp lookup SRV");
        return false;
    }

    Logger::Debug("[Renderer] ASCII window pass initialized");
    return true;
}

void Renderer::RunAsciiWindowPass() {
    if (!_windowSwapChain || !_windowRTV || !_asciiWindowPS || !_asciiWindowCB || !_charInfoSRV || !_fontAtlasSRV || !_paletteSRVWindow || !_rampLookupSRV) {
        Logger::Warning("[Renderer] RunAsciiWindowPass: required resources not initialized");
        return;
    }

    // Get back buffer size from swap chain
    DXGI_SWAP_CHAIN_DESC scDesc = {};
    if (FAILED(_windowSwapChain->GetDesc(&scDesc))) {
        Logger::Warning("[Renderer] RunAsciiWindowPass: failed to get swap chain desc");
        return;
    }

    UINT outWidth  = scDesc.BufferDesc.Width;
    UINT outHeight = scDesc.BufferDesc.Height;
    if (outWidth == 0 || outHeight == 0) {
        Logger::Warning("[Renderer] RunAsciiWindowPass: window size 0; skipping");
        return;
    }

    // Update constant buffer
    struct AsciiWindowCB {
        UINT cellsX, cellsY;
        UINT cellPixelsX, cellPixelsY;
        UINT outWidth, outHeight;
        UINT atlasSliceCount, pad0;
    };

    AsciiWindowCB cbData = {};
    cbData.cellsX           = Screen::GetInst().GetWidth();
    cbData.cellsY           = Screen::GetInst().GetHeight();
    cbData.cellPixelsX      = static_cast<UINT>(_fontAtlasCellPixelsX);
    cbData.cellPixelsY      = static_cast<UINT>(_fontAtlasCellPixelsY);
    cbData.outWidth         = outWidth;
    cbData.outHeight        = outHeight;
    cbData.atlasSliceCount  = static_cast<UINT>(_fontAtlasGlyphCount);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(_context->Map(_asciiWindowCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped.pData, &cbData, sizeof(cbData));
        _context->Unmap(_asciiWindowCB.Get(), 0);
    }

    // Bind RTV for window back buffer
    ID3D11RenderTargetView* rtv = _windowRTV.Get();
    _context->OMSetRenderTargets(1, &rtv, nullptr);

    // Set viewport to back buffer size
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<FLOAT>(outWidth);
    vp.Height   = static_cast<FLOAT>(outHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    _context->RSSetViewports(1, &vp);

    // Bind resources: t0 charInfo, t1 fontAtlas, t2 palette, t3 rampLookup
    ID3D11ShaderResourceView* srvs[4] = { _charInfoSRV.Get(), _fontAtlasSRV.Get(), _paletteSRVWindow.Get(), _rampLookupSRV.Get() };
    _context->PSSetShaderResources(0, 4, srvs);

    ID3D11SamplerState* samplers[2] = { _fontAtlasSamplerPoint.Get(), _samplerLinear.Get() };
    _context->PSSetSamplers(0, 2, samplers);

    ID3D11Buffer* cb = _asciiWindowCB.Get();
    _context->VSSetConstantBuffers(0, 1, &cb);
    _context->PSSetConstantBuffers(0, 1, &cb);

    // Use existing fullscreen quad and VS/input layout from quantization pass
    _context->VSSetShader(_quantizationVS.Get(), nullptr, 0);
    _context->PSSetShader(_asciiWindowPS.Get(), nullptr, 0);
    _context->IASetInputLayout(_quantizationInputLayout.Get());
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT stride = 2 * sizeof(float);
    UINT offset = 0;
    _context->IASetVertexBuffers(0, 1, _fullscreenQuadVB.GetAddressOf(), &stride, &offset);

    // Draw fullscreen quad
    _context->Draw(6, 0);

    // Present window swap chain
    _windowSwapChain->Present(0, 0);
}

} // namespace ASCIIgL

