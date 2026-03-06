#pragma once

#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <cstddef>
#include <memory>

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

namespace ASCIIgL {

// Forward declarations
class Shader;
class ShaderProgram;
class Material;

// ComPtr alias for cleaner code
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Palette matching mode for color LUT computation
enum class PaletteMode {
    Monochrome,   // Single hue - compare by luminance only (fast)
    MultiColor    // Full color - linearized sRGB comparison (accurate)
};

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
        bool          disableBackfaceCulling = false; // per-draw override: render both sides (toggles rasterizer state)
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
    // Single sampler with linear filtering (no anisotropic - MSAA handles AA)
    ComPtr<ID3D11SamplerState> _samplerLinear;
    ComPtr<ID3D11ShaderResourceView> _currentTextureSRV;
    
    // Texture cache: Maps Texture* -> ShaderResourceView
    // Prevents recreating GPU textures every draw call
    std::unordered_map<const Texture*, ComPtr<ID3D11ShaderResourceView>> _textureCache;
    
    // Texture array cache: Maps TextureArray* -> ShaderResourceView
    std::unordered_map<const TextureArray*, ComPtr<ID3D11ShaderResourceView>> _textureArrayCache;

    // =========================================================================
    // Rasterizer State
    // =========================================================================
    // [0-3]: Clockwise (CW), [4-7]: Counter-Clockwise (CCW)
    // Index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    ComPtr<ID3D11RasterizerState> _rasterizerStates[8];

    // =========================================================================
    // Staging Texture (for GPU->CPU framebuffer download)
    // =========================================================================
    ComPtr<ID3D11Texture2D> _stagingTexture;

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
    // Buffers and buffer methods
    // =========================================================================
    std::vector<glm::ivec4> _color_buffer;
    void OverwritePxBuffWithColBuff();

    // =========================================================================
    // Antialiasing
    // =========================================================================
    int _antialiasing_samples = 4;
    bool _antialiasing = false;

    // =========================================================================
    // Glyphs and Color LUT
    // =========================================================================
    static constexpr std::array<wchar_t, 12> _charRamp = {
        L' ', L'.', L'-', L':',
        L'o', L'+', L'O', L'A',
        L'E', L'0', L'B', L'@'
    };
    
    static constexpr std::array<float, 12> _charCoverage = {
        0.00f,   // ' '
        0.056f,  // '.'
        0.084f,  // '-'
        0.140f,  // ':'
        0.224f,  // 'o'
        0.294f,  // 'E'
        0.294f,  // '+'
        0.350f,  // 'O'
        0.434f,  // 'A'
        0.574f,  // '0'
        0.630f,  // 'B'
        0.672f   // '@'
    };

    // TODO, go throuh each character and eyeball it better than just getting chat to guess
    // or make a program that simulates the font and windows antialiasing to get the values

    void PrecomputeColorLUT();
    PaletteMode _paletteMode = PaletteMode::MultiColor;
    bool _colorLUTComputed = false;
    static constexpr unsigned int _rgbLUTDepth = 16;
    std::array<CHAR_INFO, _rgbLUTDepth*_rgbLUTDepth*_rgbLUTDepth> _colorLUT;

    // =========================================================================
    // Diagnostics
    // =========================================================================
    void ResetDiagnostics();
    void LogDiagnostics() const;
    bool _diagnostics_enabled = false;

    // =========================================================================
    // Helper Drawing Functions
    // =========================================================================
    void DrawClippedLinePxBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, WCHAR pixel_type, unsigned short col);
    void DrawClippedLineColBuff(int x0, int y0, int x1, int y1, int minX, int maxX, int minY, int maxY, const glm::ivec4& col);

    // =========================================================================
    // GPU Initialization Methods
    // =========================================================================
    bool InitializeDevice();
    bool InitializeRenderTarget();
    bool InitializeDepthStencil();
    bool InitializeSamplers();
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
    void Initialize(bool antialiasing = false, int antialiasing_samples = 4);
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
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::ivec4& col);
    void DrawLineColBuff(int x1, int y1, int x2, int y2, const glm::ivec3& col);

    void DrawTriangleWireframePxBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, WCHAR pixel_type, const unsigned short col);
    void DrawTriangleWireframeColBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::ivec4& col);
    void DrawTriangleWireframeColBuff(const glm::vec2& vert1, const glm::vec2& vert2, const glm::vec2& vert3, const glm::ivec3& col);

    void DrawScreenBorderPxBuff(const unsigned short col);
    void DrawScreenBorderColBuff(const glm::vec3& col);

    void PlotColor(int x, int y, const glm::ivec3& color);
    void PlotColor(int x, int y, const glm::ivec4& color);
    void PlotColorBlend(int x, int y, const glm::ivec4& color);

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

    void SetDiagnosticsEnabled(const bool enabled);
    bool GetDiagnosticsEnabled() const;

    // Palette Mode for Color LUT
    void SetPaletteMode(PaletteMode mode);
    PaletteMode GetPaletteMode() const;
    
    // =========================================================================
    // Buffer and Diagnostics
    // =========================================================================
    std::vector<glm::ivec4>& GetColorBuffer();

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
    void BindTexture(const Texture* tex, int slot = 0);
    void BindTextureArray(const TextureArray* texArray, int slot = 0);
    
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
