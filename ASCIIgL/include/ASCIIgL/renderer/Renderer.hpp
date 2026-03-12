#pragma once

#include <vector>
#include <array>
#include <cstddef>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#include <ASCIIgL/engine/Mesh.hpp>
#include <ASCIIgL/engine/Model.hpp>
#include <ASCIIgL/engine/Texture.hpp>
#include <ASCIIgL/engine/TextureArray.hpp>
#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>

#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>
#include <ASCIIgL/renderer/Shader.hpp>  // for UniformValue, UniformDescriptor
#include <ASCIIgL/renderer/SamplerType.hpp>

namespace ASCIIgL {

// Forward declarations
class Shader;
class ShaderProgram;
class Material;

// ComPtr alias for cleaner code
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class Renderer
{
    friend class Shader;
    friend class ShaderProgram;
    friend class Material;

public:
    // =========================================================================
    // Draw Call Queue Types
    // =========================================================================
    struct UniformOverride {
        const UniformDescriptor* desc = nullptr;  // layout metadata (offset/size/type)
        UniformValue             value;          // concrete value to write
    };

    struct DrawCall {
        const Mesh*   mesh        = nullptr;
        Material*     material    = nullptr;   // non-owning
        int           layer       = 0;
        bool          transparent = false;     // false = opaque pass, true = transparent pass
        float         sortKey     = 0.0f;      // used for transparent sorting (e.g. depth or layer)
        std::vector<UniformOverride> overrides; // per-draw uniform overrides
    };

private:
    bool _initialized = false;

    // =========================================================================
    // Singleton and Construction
    // =========================================================================
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // =========================================================================
    // Buffer Configuration Constants
    // =========================================================================
    static constexpr size_t INITIAL_VERTEX_BUFFER_CAPACITY = 10000;
    static constexpr size_t BUFFER_GROWTH_FACTOR = 2;

    // =========================================================================
    // DirectX 11 Core Resources
    // =========================================================================
    ComPtr<ID3D11Device> _device;
    ComPtr<ID3D11DeviceContext> _context;
    ComPtr<ID3D11Texture2D> _renderTarget;          // MSAA render target
    ComPtr<ID3D11RenderTargetView> _renderTargetView;
    ComPtr<ID3D11Texture2D> _resolvedTexture;       // Non-MSAA resolved texture for CPU download
    ComPtr<ID3D11Texture2D> _depthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> _depthStencilView;
    ComPtr<ID3D11DepthStencilState> _depthStencilState;
    ComPtr<ID3D11DepthStencilState> _depthStencilStateNoTest;   // Depth disabled
    ComPtr<ID3D11DepthStencilState> _depthStencilStateNoWrite; // Depth test on, write off (transparent/2D)
    ComPtr<ID3D11BlendState> _blendStateOpaque;   // No blending (default)
    ComPtr<ID3D11BlendState> _blendStateAlpha;   // Alpha blending for GUI (SrcAlpha, InvSrcAlpha)

    // Debug swap chain for RenderDoc support
    ComPtr<IDXGISwapChain> _debugSwapChain;
    HWND _debugWindow = nullptr;

    // =========================================================================
    // Per-Mesh GPU Buffer Cache
    // =========================================================================
    struct GPUMeshCache {
        ComPtr<ID3D11Buffer> vertexBuffer;
        ComPtr<ID3D11Buffer> indexBuffer;
        size_t vertexCount = 0;
        size_t indexCount = 0;
    };
    GPUMeshCache* GetOrCreateMeshCache(const Mesh* mesh);

    // =========================================================================
    // Texture Resources
    // =========================================================================
    ComPtr<ID3D11SamplerState> _samplerLinear;
    ComPtr<ID3D11SamplerState> _samplerAnisotropic;
    ComPtr<ID3D11ShaderResourceView> _currentTextureSRV;
    
    std::unordered_map<const Texture*, ComPtr<ID3D11ShaderResourceView>> _textureCache;
    std::unordered_map<const TextureArray*, ComPtr<ID3D11ShaderResourceView>> _textureArrayCache;

    // =========================================================================
    // Rasterizer State
    // =========================================================================
    // [0-3]: Clockwise (CW), [4-7]: Counter-Clockwise (CCW)
    // Index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    ComPtr<ID3D11RasterizerState> _rasterizerStates[8];

    // =========================================================================
    // Staging Texture (for GPU->CPU CHAR_INFO download)
    // =========================================================================
    ComPtr<ID3D11Texture2D> _stagingTexture;

    // =========================================================================
    // Quantization pass (color RT -> CHAR_INFO RT)
    // =========================================================================
    ComPtr<ID3D11Texture2D> _charInfoTexture;       // R16G16_UINT render target
    ComPtr<ID3D11RenderTargetView> _charInfoRTV;
    ComPtr<ID3D11ShaderResourceView> _resolvedTextureSRV;  // resolved color as SRV for quantization
    ComPtr<ID3D11ShaderResourceView> _colorLUTSRV;        // 4096x1 R16G16_UINT (multi)
    ComPtr<ID3D11ShaderResourceView> _monochromeLUTSRV;   // 1024x1 R16G16_UINT (mono)
    ComPtr<ID3D11Texture1D> _colorLUTTexture;
    ComPtr<ID3D11Texture1D> _monochromeLUTTexture;
    ComPtr<ID3D11Buffer> _lutConstantsCB;          // Lmin, Lmax, isMonochrome (for quantization FS)
    ComPtr<ID3D11VertexShader> _quantizationVS;
    ComPtr<ID3D11PixelShader> _quantizationPS;
    ComPtr<ID3D11InputLayout> _quantizationInputLayout;
    ComPtr<ID3D11Buffer> _fullscreenQuadVB;       // for quantization fullscreen draw
    void RunQuantizationPass();
    void UploadLUTsToGPU();
    bool EnsureQuantizationResources();
    bool InitializeCharInfoTarget();
    bool InitializeQuantizationShaders();

    // =========================================================================
    // Currently Bound Shader Program (nullptr = default)
    // =========================================================================
    ShaderProgram* _boundShaderProgram = nullptr;
    Material* _boundMaterial = nullptr;

    // =========================================================================
    // Rendering Settings
    // =========================================================================
    bool _wireframe = false;
    bool _backface_culling = true;
    bool _ccw = false;

    glm::ivec3 _background_col = glm::ivec3(0, 0, 0);

    // =========================================================================
    // Buffer methods (no CPU color buffer; output is CHAR_INFO via quantization pass)
    // =========================================================================

    // =========================================================================
    // Antialiasing
    // =========================================================================
    int _antialiasing_samples = 4;
    bool _antialiasing = false;

    // Anisotropic filtering level for texture arrays (1, 2, 4, 8, 16). 1 = off.
    int _maxAnisotropy = 16;

    // =========================================================================
    // Glyphs and Color LUT
    // =========================================================================
    std::vector<wchar_t> _charRamp;
    std::vector<float> _charCoverage;

    void LoadCharCoverageFromJson(const wchar_t* charRamp = nullptr, int charRampCount = 10);
    void PrecomputeColorLUT();
    void PrecomputeMonochromeColorLUT(Palette& palette);
    void PrecomputeMultiColorLUT(Palette& palette);
    /// Map luminance L to index in [0, 1023] for monochrome LUT lookup.
    size_t MonochromeLuminanceToIndex(float L) const;

    enum class ColorLUTState { NotComputed, Monochrome, MultiColor };
    ColorLUTState _colorLUTState = ColorLUTState::NotComputed;
    static constexpr unsigned int _rgbLUTDepth = 16; // DO NOT CHANGE THIS VALUE (WILL BREAK THE LUT)
    std::array<CHAR_INFO, _rgbLUTDepth*_rgbLUTDepth*_rgbLUTDepth> _colorLUT;

    static constexpr size_t _monochromeLUTSize = 1024;
    // Each entry stores (target luminance, CHAR_INFO)
    std::array<std::pair<float, CHAR_INFO>, _monochromeLUTSize> _monochromeLUT;

    bool _monochromeDitherEnabled = false;

    // =========================================================================
    // Helper Drawing Functions
    // =========================================================================
    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col);

    // =========================================================================
    // GPU Initialization Methods
    // =========================================================================
    bool InitializeDevice();
    bool InitializeRenderTarget();
    bool InitializeDepthStencil();
    bool InitializeSamplers();
    bool CreateAnisotropicSampler();  // (re)creates _samplerAnisotropic using _maxAnisotropy
    bool InitializeRasterizerStates();
    bool InitializeBlendStates();
    bool InitializeStagingTexture();
    bool InitializeDebugSwapChain();  // For RenderDoc support

    // =========================================================================
    // Texture Management (Internal)
    // =========================================================================
    bool CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv);
    bool CreateTextureArraySRV(const TextureArray* texArray, ID3D11ShaderResourceView** srv);

    // =========================================================================
    // Cleanup
    // =========================================================================
    void Shutdown();

    // =========================================================================
    // Frame Management (Internal)
    // =========================================================================
    void DownloadFramebuffer();
    void BeginColBuffFrame();
    void EndColBuffFrame();

    // Immediate GPU draw primitive, used internally by the draw-call system.
    void DrawMesh(const Mesh* mesh);

    // =========================================================================
    // Draw Call Queues
    // =========================================================================
    std::vector<DrawCall> _opaqueDraws;
    std::vector<DrawCall> _transparentDraws;
    void SortOpaqueDraws();
    void SortTransparentDraws();
    void ExecuteDrawList(const std::vector<DrawCall>& list);

public:
    // =========================================================================
    // Singleton Access
    // =========================================================================
    static Renderer& GetInst() {
        static Renderer instance;
        return instance;
    }

    // =========================================================================
    // Initialization and Core API
    // =========================================================================
    /// charRamp: custom chars for coverage; nullptr or empty = use default ramp.
    /// charRampCount: when using default ramp, subsample to this many chars with evenly spaced coverage (default 10); ignored if charRamp is set.
    void Initialize(bool antialiasing = false, int antialiasing_samples = 4, const wchar_t* charRamp = nullptr, int charRampCount = 10);
    bool IsInitialized() const;

    // =========================================================================
    // Drawing API (queued)
    // =========================================================================
    // Enqueue all meshes of a model as draw calls using the given material and MVP.
    void DrawModel(const Model& model,
                   Material* material,
                   const glm::mat4& mvp,
                   int layer,
                   bool transparent,
                   float sortKey = 0.0f);

    // =========================================================================
    // Low-Level Drawing API - Primitives, No pipeline involved
    // =========================================================================
    void DrawLinePxBuff(int x1, int y1, int x2, int y2, WCHAR pixel_type, const unsigned short col);

    void DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, WCHAR pixel_type, const unsigned short col);

    void DrawScreenBorderPxBuff(const unsigned short col);

    // =========================================================================
    // Utility
    // =========================================================================
    CHAR_INFO GetCharInfo(const glm::ivec3& rgb);

    // =========================================================================
    // Settings API
    // =========================================================================
    int GetAntialiasingsamples() const;

    void SetWireframe(const bool wireframe);
    bool GetWireframe() const;

    void SetBackfaceCulling(const bool backfaceCulling);
    bool GetBackfaceCulling() const;

    void SetCCW(const bool ccw);
    bool GetCCW() const;

    bool GetAntialiasing() const;

    glm::ivec3 GetBackgroundCol() const;
    void SetBackgroundCol(const glm::ivec3& color);

    /// Enables 4x4 Bayer ordered dithering for the monochrome LUT path.
    /// Default: false.
    void SetMonochromeDitherEnabled(bool enabled) { _monochromeDitherEnabled = enabled; }
    bool GetMonochromeDitherEnabled() const { return _monochromeDitherEnabled; }

    /// Anisotropic filtering level for texture arrays. Valid: 1 (off), 2, 4, 8, 16. Default 16.
    void SetMaxAnisotropy(int level);
    int GetMaxAnisotropy() const;

    // =========================================================================
    // GPU Public API
    // =========================================================================
    
    // Resource cache cleanup (called by resource destructors)
    void ReleaseMeshCache(void* cachePtr);
    void InvalidateCachedTexture(const Texture* tex);
    
    // Bind a custom shader program (nullptr to use default)
    void BindShaderProgram(ShaderProgram* program);
    
    // Bind a material (sets shader + uniforms + textures)
    void BindMaterial(Material* material);
    
    // Unbind custom shader (reverts to default)
    void UnbindShaderProgram();
    
    // Get currently bound shader program (nullptr if using default)
    ShaderProgram* GetBoundShaderProgram() const { return _boundShaderProgram; }

    // Helper: Upload material's constant buffer
    void UploadMaterialConstants(Material* material);

    // Texture Management (Public)
    void BindTexture(const Texture* tex, int slot = 0, SamplerType type = SamplerType::Default);
    void BindTextureArray(const TextureArray* texArray, int slot = 0, SamplerType type = SamplerType::Default);
    
    // Unbind any texture
    void UnbindTexture(int slot = 0);
    void UnbindTextureArray(int slot = 0);

    /// Set depth test on/off. Use false for 2D overlay (GUI) so it is not clipped by 3D depth buffer.
    void SetDepthTestEnabled(bool enabled);

    /// Set depth write on/off. Use false for transparent/2D so they don't occlude geometry behind.
    void SetDepthWriteEnabled(bool enabled);

    /// Set alpha blending on/off. Use true for 2D GUI so transparent PNG regions blend correctly.
    void SetBlendEnabled(bool enabled);

    // =========================================================================
    // Draw Call Queue API / GPU Frame
    // =========================================================================
    // Clears internal draw queues and begins the GPU render pass for this frame.
    void BeginGpuFrame();
    // Enqueue a draw call; actual GPU draws are issued during FlushDraws().
    void SubmitDraw(const DrawCall& call);
    // Execute queued draws in two passes: opaque then transparent.
    void FlushDraws();
    // End the GPU render pass for this frame (resolve + download).
    void EndGpuFrame();
};

} // namespace ASCIIgL
