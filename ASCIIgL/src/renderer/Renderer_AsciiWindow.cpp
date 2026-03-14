#include <ASCIIgL/renderer/Renderer.hpp>

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
    uint2 g_padding;
};

Texture2D<uint2>       g_charInfo  : register(t0); // glyph, attrs
Texture2DArray<float4> g_fontAtlas : register(t1); // RGBA, alpha in A
Texture1D<float4>      g_palette   : register(t2); // RGBA colors

SamplerState g_samplerPoint  : register(s0);
SamplerState g_samplerLinear : register(s1);

float4 main(float4 svPos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    uint2 pix = uint2(svPos.xy);

    uint2 cell = pix / g_cellPixels;
    uint2 inCell = pix % g_cellPixels;

    cell = min(cell, g_cells - 1);

    uint2 ci = g_charInfo.Load(int3(cell, 0));
    uint glyphIndex = ci.x;
    uint attrs      = ci.y;

    uint fgIndex = attrs & 0x0F;
    uint bgIndex = (attrs >> 4) & 0x0F;

    float2 atlasUV = (float2(inCell) + 0.5) / float2(g_cellPixels);
    float4 glyphSample = g_fontAtlas.SampleLevel(g_samplerPoint, float3(atlasUV, glyphIndex), 0.0);
    float alpha = glyphSample.a;

    float2 fgCoord = (float(fgIndex) + 0.5) / 16.0;
    float2 bgCoord = (float(bgIndex) + 0.5) / 16.0;
    float3 fg = g_palette.SampleLevel(g_samplerLinear, fgCoord.x, 0.0).rgb;
    float3 bg = g_palette.SampleLevel(g_samplerLinear, bgCoord.x, 0.0).rgb;

    float3 rgb = lerp(bg, fg, alpha);
    return float4(rgb, 1.0);
}
)";

    // Compile pixel shader
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3DCompile(
        ASCII_WINDOW_PS_SRC,
        std::strlen(ASCII_WINDOW_PS_SRC),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
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

    // Create constant buffer
    struct AsciiWindowCB {
        UINT cellsX, cellsY;
        UINT cellPixelsX, cellPixelsY;
        UINT outWidth, outHeight;
        UINT pad0, pad1;
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

    // Create palette 1D texture from current Screen palette (16 entries)
    Palette& palette = Screen::GetInst().GetPalette();
    const UINT paletteSize = Palette::COLOR_COUNT;
    std::vector<float> paletteData(paletteSize * 4);
    for (UINT i = 0; i < paletteSize; ++i) {
        glm::vec3 n = palette.GetRGBNormalized(i);
        paletteData[i * 4 + 0] = n.r;
        paletteData[i * 4 + 1] = n.g;
        paletteData[i * 4 + 2] = n.b;
        paletteData[i * 4 + 3] = 1.0f;
    }

    D3D11_TEXTURE1D_DESC texDesc = {};
    texDesc.Width = paletteSize;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = paletteData.data();
    initData.SysMemPitch = sizeof(float) * 4 * paletteSize;

    _paletteTextureWindow.Reset();
    hr = _device->CreateTexture1D(&texDesc, &initData, &_paletteTextureWindow);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create window palette texture");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc = {};
    paletteSrvDesc.Format = texDesc.Format;
    paletteSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    paletteSrvDesc.Texture1D.MipLevels = 1;
    paletteSrvDesc.Texture1D.MostDetailedMip = 0;

    _paletteSRVWindow.Reset();
    hr = _device->CreateShaderResourceView(_paletteTextureWindow.Get(), &paletteSrvDesc, &_paletteSRVWindow);
    if (FAILED(hr)) {
        Logger::Error("[Renderer] Failed to create window palette SRV");
        return false;
    }

    Logger::Debug("[Renderer] ASCII window pass initialized");
    return true;
}

void Renderer::RunAsciiWindowPass() {
    if (!_windowSwapChain || !_windowRTV || !_asciiWindowPS || !_asciiWindowCB || !_charInfoSRV || !_fontAtlasSRV || !_paletteSRVWindow) {
        Logger::Warning("[Renderer] RunAsciiWindowPass: required resources not initialized");
        return;
    }

    // Get back buffer size from swap chain
    DXGI_SWAP_CHAIN_DESC scDesc = {};
    if (FAILED(_windowSwapChain->GetDesc(&scDesc))) {
        Logger::Warning("[Renderer] RunAsciiWindowPass: failed to get swap chain desc");
        return;
    }

    const UINT outWidth  = scDesc.BufferDesc.Width;
    const UINT outHeight = scDesc.BufferDesc.Height;

    // Update constant buffer
    struct AsciiWindowCB {
        UINT cellsX, cellsY;
        UINT cellPixelsX, cellPixelsY;
        UINT outWidth, outHeight;
        UINT pad0, pad1;
    };

    AsciiWindowCB cbData = {};
    cbData.cellsX      = Screen::GetInst().GetWidth();
    cbData.cellsY      = Screen::GetInst().GetHeight();
    cbData.cellPixelsX = static_cast<UINT>(_fontAtlasCellPixelsX);
    cbData.cellPixelsY = static_cast<UINT>(_fontAtlasCellPixelsY);
    cbData.outWidth    = outWidth;
    cbData.outHeight   = outHeight;

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

    // Bind resources
    ID3D11ShaderResourceView* srvs[3] = { _charInfoSRV.Get(), _fontAtlasSRV.Get(), _paletteSRVWindow.Get() };
    _context->PSSetShaderResources(0, 3, srvs);

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

