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
#include <ASCIIgL/engine/Camera3D.hpp>
#include <ASCIIgL/engine/Camera2D.hpp>

#include <ASCIIgL/renderer/screen/Screen.hpp>

#include <ASCIIgL/renderer/Palette.hpp>
#include <ASCIIgL/renderer/VertFormat.hpp>

namespace ASCIIgL {

// Forward declarations
class Renderer;
class Shader;
class ShaderProgram;
class Material;

// ComPtr alias for cleaner code
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class RendererGPU
{
    friend class Renderer;
    friend class Shader;
    friend class ShaderProgram;
    friend class Material;

private:
    // =========================================================================
    // Buffer Configuration Constants
    // =========================================================================
    static constexpr size_t INITIAL_VERTEX_BUFFER_CAPACITY = 10000;
    static constexpr size_t BUFFER_GROWTH_FACTOR = 2;

    // =========================================================================
    // Singleton Pattern (Non-copyable, Non-movable)
    // =========================================================================
    RendererGPU() = default;
    ~RendererGPU();
    RendererGPU(const RendererGPU&) = delete;
    RendererGPU& operator=(const RendererGPU&) = delete;

    // =========================================================================
    // Initialization State
    // =========================================================================
    void Initialize();
    bool _initialized = false;
    Renderer* _renderer = nullptr;

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

    // =========================================================================
    // Rasterizer State
    // =========================================================================
    // [0-3]: Clockwise (CW), [4-7]: Counter-Clockwise (CCW)
    // Index: wireframe(0/1) + cull(0/2) + ccw(0/4)
    ComPtr<ID3D11RasterizerState> _rasterizerStates[8];

    // =========================================================================
    // Static Quad Meshes (for 2D rendering)
    // =========================================================================
    Mesh* _quadMeshCCW = nullptr;
    Mesh* _quadMeshCW = nullptr;
    void InitializeQuadMeshes();
    void CleanupQuadMeshes();

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
    // Initialization Methods
    // =========================================================================
    bool InitializeDevice();
    bool InitializeRenderTarget();
    bool InitializeDepthStencil();
    bool InitializeSamplers();
    bool InitializeRasterizerStates();
    bool InitializeStagingTexture();
    bool InitializeDebugSwapChain();  // For RenderDoc support

    // =========================================================================
    // Texture Management
    // =========================================================================
    bool CreateTextureFromASCIIgLTexture(const Texture* tex, ID3D11ShaderResourceView** srv);
    void BindTexture(const Texture* tex);
    void UnbindTexture();

    // =========================================================================
    // Cleanup
    // =========================================================================
    void Shutdown();

    // =========================================================================
    // Frame Management
    // =========================================================================
    void BeginColBuffFrame();

    // =========================================================================
    // High-Level Drawing API - (Friend-Accessible Only via Renderer)
    // =========================================================================
    void DrawMesh(const Mesh* mesh);
    void DrawModel(const Model& ModelObj);
    void Draw2DQuad(const Texture& tex);

public:
    // =========================================================================
    // Singleton Access (Friend-Accessible Only via Renderer)
    // =========================================================================
    static RendererGPU& GetInst() {
        static RendererGPU instance;
        return instance;
    }

    // =========================================================================
    // Public API
    // =========================================================================
    bool IsInitialized() const { return _initialized; }
    
    // Resource cache cleanup (called by resource destructors)
    void ReleaseMeshCache(void* cachePtr);
    void InvalidateTextureCache(const Texture* tex);

    // =========================================================================
    // Frame Management
    // =========================================================================
    void EndColBuffFrame();  // Presents debug swap chain for RenderDoc
    void DownloadFramebuffer();
    
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
};

} // namespace ASCIIgL