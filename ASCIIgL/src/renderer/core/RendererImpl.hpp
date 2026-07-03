#pragma once

// Internal definition of Renderer::Impl and Renderer::GPUMeshCache (not for public include).

#include <array>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <glm/glm.hpp>

#include <ASCIIgL/renderer/Renderer.hpp>

namespace ASCIIgL {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct Renderer::GPUMeshCache {
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    size_t vertexCount = 0;
    size_t indexCount = 0;
};

struct Renderer::Impl {
    bool _initialized = false;
    bool _supersample2x = false;
    int _renderTargetWidth = 0;
    int _renderTargetHeight = 0;

    ComPtr<ID3D11Device> _device;
    ComPtr<ID3D11DeviceContext> _context;
    ComPtr<ID3D11Texture2D> _renderTarget;
    ComPtr<ID3D11RenderTargetView> _renderTargetView;
    ComPtr<ID3D11ShaderResourceView> _renderTargetSRV;
    ComPtr<ID3D11Texture2D> _resolvedTexture;
    ComPtr<ID3D11RenderTargetView> _resolvedTextureRTV;
    ComPtr<ID3D11Texture2D> _depthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> _depthStencilView;
    ComPtr<ID3D11DepthStencilState> _depthStencilState;
    ComPtr<ID3D11DepthStencilState> _depthStencilStateNoTest;
    ComPtr<ID3D11DepthStencilState> _depthStencilStateNoWrite;
    ComPtr<ID3D11BlendState> _blendStateOpaque;
    ComPtr<ID3D11BlendState> _blendStateAlpha;

    ComPtr<IDXGISwapChain> _debugSwapChain;
    HWND _debugWindow = nullptr;

    ComPtr<IDXGISwapChain> _windowSwapChain;
    ComPtr<ID3D11RenderTargetView> _windowRTV;

    ComPtr<ID3D11SamplerState> _samplerLinear;
    ComPtr<ID3D11SamplerState> _samplerAnisotropic;
    ComPtr<ID3D11ShaderResourceView> _currentTextureSRV;

    std::unordered_map<const Texture*, ComPtr<ID3D11ShaderResourceView>> _textureCache;
    std::unordered_map<const TextureArray*, ComPtr<ID3D11ShaderResourceView>> _textureArrayCache;

    ComPtr<ID3D11RasterizerState> _rasterizerStates[8];

    ComPtr<ID3D11Texture2D> _stagingTexture;

    ComPtr<ID3D11Texture2D> _charInfoTexture;
    ComPtr<ID3D11RenderTargetView> _charInfoRTV;
    ComPtr<ID3D11ShaderResourceView> _charInfoSRV;
    ComPtr<ID3D11ShaderResourceView> _resolvedTextureSRV;
    ComPtr<ID3D11ShaderResourceView> _colorLUTSRV;
    ComPtr<ID3D11ShaderResourceView> _monochromeLUTSRV;
    ComPtr<ID3D11Texture1D> _colorLUTTexture;
    ComPtr<ID3D11Texture1D> _monochromeLUTTexture;
    ComPtr<ID3D11Buffer> _lutConstantsCB;
    ComPtr<ID3D11VertexShader> _quantizationVS;
    ComPtr<ID3D11PixelShader> _quantizationPS;
    ComPtr<ID3D11PixelShader> _downsamplePS;
    ComPtr<ID3D11InputLayout> _quantizationInputLayout;
    ComPtr<ID3D11Buffer> _fullscreenQuadVB;

    ComPtr<ID3D11Texture2D> _fontAtlasTexture;
    ComPtr<ID3D11ShaderResourceView> _fontAtlasSRV;
    ComPtr<ID3D11SamplerState> _fontAtlasSamplerPoint;
    int _fontAtlasCellPixelsX = 0;
    int _fontAtlasCellPixelsY = 0;
    int _fontAtlasGlyphCount = 0;

    ComPtr<ID3D11PixelShader> _asciiWindowPS;
    ComPtr<ID3D11Buffer> _asciiWindowCB;
    ComPtr<ID3D11Buffer> _paletteBufferWindow;
    ComPtr<ID3D11ShaderResourceView> _paletteSRVWindow;
    ComPtr<ID3D11Buffer> _rampLookupBuffer;
    ComPtr<ID3D11ShaderResourceView> _rampLookupSRV;

    ShaderProgram* _boundShaderProgram = nullptr;
    Material* _boundMaterial = nullptr;
    BoundState _boundState;

    bool _wireframe = false;
    bool _backface_culling = true;
    bool _ccw = false;

    glm::ivec3 _background_col = glm::ivec3(0, 0, 0);

    int _maxAnisotropy = 16;

    std::vector<wchar_t> _charRamp;
    std::vector<float> _charCoverage;

    enum class ColorLUTState { NotComputed, Monochrome, MultiColor };
    ColorLUTState _colorLUTState = ColorLUTState::NotComputed;
    static constexpr unsigned int _rgbLUTDepth = 16;
    std::array<ScreenPixel, _rgbLUTDepth * _rgbLUTDepth * _rgbLUTDepth> _colorLUT{};

    static constexpr size_t _monochromeLUTSize = 1024;
    std::array<std::pair<float, ScreenPixel>, _monochromeLUTSize> _monochromeLUT{};

    bool _ditheringEnabled = false;
    bool _lutGpuResourcesDirty = true;

    std::vector<DrawCall> _opaqueDraws;
    std::vector<DrawCall> _transparentDraws;
};

}  // namespace ASCIIgL
